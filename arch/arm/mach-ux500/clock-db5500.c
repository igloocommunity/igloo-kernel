/*
 *  Copyright (C) 2009 ST-Ericsson SA
 *  Copyright (C) 2009 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mfd/ab8500/sysctrl.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include "prcmu-regs-db8500.h"

#include <plat/pincfg.h>

#include <mach/hardware.h>
#include <mach/prcmu-fw-api.h>

#include "clock.h"
#include "pins-db8500.h"

#define PRCM_SDMMCCLK_MGT	0x024
#define SD_CLK_DIV_MASK		0x1F
#define SD_CLK_DIV_VAL		8

static DEFINE_MUTEX(sysclk_mutex);
static DEFINE_MUTEX(ab_ulpclk_mutex);
static DEFINE_MUTEX(audioclk_mutex);

static struct delayed_work sysclk_disable_work;

/* PLL operations. */

static int clk_pllsrc_enable(struct clk *clk)
{
	/* To enable pll */
	return 0;
}

static void clk_pllsrc_disable(struct clk *clk)
{
	/* To disable pll */
}

static struct clkops pll_clk_ops = {
	.enable = clk_pllsrc_enable,
	.disable = clk_pllsrc_disable,
};

/* SysClk operations. */

static int request_sysclk(bool enable)
{
	static int requests;

	if ((enable && (requests++ == 0)) || (!enable && (--requests == 0)))
		return prcmu_request_clock(PRCMU_SYSCLK, enable);
	return 0;
}

static int sysclk_enable(struct clk *clk)
{
	static bool swat_enable;
	int r;

	if (!swat_enable) {
		r = ab8500_sysctrl_set(AB8500_SWATCTRL,
			AB8500_SWATCTRL_SWATENABLE);
		if (r)
			return r;

		swat_enable = true;
	}

	r = request_sysclk(true);
	if (r)
		return r;

	if (clk->cg_sel) {
		r = ab8500_sysctrl_set(AB8500_SYSULPCLKCTRL1, (u8)clk->cg_sel);
		if (r)
			(void)request_sysclk(false);
	}
	return r;
}

static void sysclk_disable(struct clk *clk)
{
	int r;

	if (clk->cg_sel) {
		r = ab8500_sysctrl_clear(AB8500_SYSULPCLKCTRL1,
			(u8)clk->cg_sel);
		if (r)
			goto disable_failed;
	}
	r = request_sysclk(false);
	if (r)
		goto disable_failed;
	return;

disable_failed:
	pr_err("clock: failed to disable %s.\n", clk->name);
}

static struct clkops sysclk_ops = {
	.enable = sysclk_enable,
	.disable = sysclk_disable,
};

/* AB8500 UlpClk operations */

static int ab_ulpclk_enable(struct clk *clk)
{
	int err;

	if (clk->regulator == NULL) {
		struct regulator *reg;

		reg = regulator_get(NULL, "v-intcore");
		if (IS_ERR(reg))
			return PTR_ERR(reg);
		clk->regulator = reg;
	}
	err = regulator_enable(clk->regulator);
	if (err)
		return err;
	err = ab8500_sysctrl_clear(AB8500_SYSULPCLKCONF,
		AB8500_SYSULPCLKCONF_ULPCLKCONF_MASK);
	if (err)
		return err;
	return ab8500_sysctrl_set(AB8500_SYSULPCLKCTRL1,
		AB8500_SYSULPCLKCTRL1_ULPCLKREQ);
}

static void ab_ulpclk_disable(struct clk *clk)
{
	if (ab8500_sysctrl_clear(AB8500_SYSULPCLKCTRL1,
		AB8500_SYSULPCLKCTRL1_ULPCLKREQ))
		goto out_err;
	if (clk->regulator != NULL) {
		if (regulator_disable(clk->regulator))
			goto out_err;
	}
	return;

out_err:
	pr_err("clock: %s failed to disable %s.\n", __func__, clk->name);
}

static struct clkops ab_ulpclk_ops = {
	.enable = ab_ulpclk_enable,
	.disable = ab_ulpclk_disable,
};

/* AB8500 audio clock operations */

static int audioclk_enable(struct clk *clk)
{
	return ab8500_sysctrl_set(AB8500_SYSULPCLKCTRL1,
		AB8500_SYSULPCLKCTRL1_AUDIOCLKENA);
}

static void audioclk_disable(struct clk *clk)
{
	if (ab8500_sysctrl_clear(AB8500_SYSULPCLKCTRL1,
		AB8500_SYSULPCLKCTRL1_AUDIOCLKENA)) {
		pr_err("clock: %s failed to disable %s.\n", __func__,
			clk->name);
	}
}

static int audioclk_set_parent(struct clk *clk, struct clk *parent)
{
	if (parent->ops == &sysclk_ops) {
		return ab8500_sysctrl_clear(AB8500_SYSULPCLKCTRL1,
			AB8500_SYSULPCLKCTRL1_SYSULPCLKINTSEL_MASK);
	} else if (parent->ops == &ab_ulpclk_ops) {
		return ab8500_sysctrl_write(AB8500_SYSULPCLKCTRL1,
			AB8500_SYSULPCLKCTRL1_SYSULPCLKINTSEL_MASK,
			(1 << AB8500_SYSULPCLKCTRL1_SYSULPCLKINTSEL_SHIFT));
	} else {
		return -EINVAL;
	}
}

static struct clkops audioclk_ops = {
	.enable = audioclk_enable,
	.disable = audioclk_disable,
	.set_parent = audioclk_set_parent,
};

/* Primary camera clock operations */
static int clkout0_enable(struct clk *clk)
{
	int r;

	r = prcmu_config_clkout(0, PRCMU_CLKSRC_SYSCLK, 4);
	if (r)
		return r;
	return nmk_config_pin(GPIO227_CLKOUT1, false);
}

static void clkout0_disable(struct clk *clk)
{
	int r;

	r = nmk_config_pin(GPIO227_GPIO, false);
	if (r)
		goto disable_failed;
	r = prcmu_config_clkout(0, PRCMU_CLKSRC_SYSCLK, 0);
	if (!r)
		return;
disable_failed:
	pr_err("clock: failed to disable %s.\n", clk->name);
}

/* Touch screen/secondary camera clock operations. */
static int clkout1_enable(struct clk *clk)
{
	int r;

	r = prcmu_config_clkout(1, PRCMU_CLKSRC_SYSCLK, 4);
	if (r)
		return r;
	return nmk_config_pin(GPIO228_CLKOUT2, false);
}

static void clkout1_disable(struct clk *clk)
{
	int r;

	r = nmk_config_pin(GPIO228_GPIO, false);
	if (r)
		goto disable_failed;
	r = prcmu_config_clkout(1, PRCMU_CLKSRC_SYSCLK, 0);
	if (!r)
		return;
disable_failed:
	pr_err("clock: failed to disable %s.\n", clk->name);
}

static struct clkops clkout0_ops = {
	.enable = clkout0_enable,
	.disable = clkout0_disable,
};

static struct clkops clkout1_ops = {
	.enable = clkout1_enable,
	.disable = clkout1_disable,
};

#define DEF_PER1_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U8500_CLKRST1_BASE, _cg_bit, &per1clk)
#define DEF_PER2_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U8500_CLKRST2_BASE, _cg_bit, &per2clk)
#define DEF_PER3_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U8500_CLKRST3_BASE, _cg_bit, &per3clk)
#define DEF_PER5_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U8500_CLKRST5_BASE, _cg_bit, &per5clk)
#define DEF_PER6_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U8500_CLKRST6_BASE, _cg_bit, &per6clk)
#define DEF_PER7_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U8500_CLKRST7_BASE_ED, _cg_bit, &per7clk)

#define DEF_PER1_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U8500_CLKRST1_BASE, _cg_bit, _parent)
#define DEF_PER2_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U8500_CLKRST2_BASE, _cg_bit, _parent)
#define DEF_PER3_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U8500_CLKRST3_BASE, _cg_bit, _parent)
#define DEF_PER5_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U8500_CLKRST5_BASE, _cg_bit, _parent)
#define DEF_PER6_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U8500_CLKRST6_BASE, _cg_bit, _parent)
#define DEF_PER7_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U8500_CLKRST7_BASE_ED, _cg_bit, _parent)

#define DEF_MTU_CLK(_cg_sel, _name, _bus_parent) \
	struct clk _name = { \
		.name = #_name, \
		.ops = &mtu_clk_ops, \
		.cg_sel = _cg_sel, \
		.bus_parent = _bus_parent, \
	}

/* Clock sources. */

static struct clk soc0_pll = {
	.name = "soc0_pll",
	.ops = &pll_clk_ops,
};

static struct clk soc1_pll = {
	.name = "soc1_pll",
	.ops = &pll_clk_ops,
};

static struct clk ddr_pll = {
	.name = "ddr_pll",
	.ops = &pll_clk_ops,
};

static struct clk ulp38m4 = {
	.name = "ulp38m4",
};

static struct clk sysclk = {
	.name = "sysclk",
	.ops = &sysclk_ops,
	.rate = 38400000,
	.mutex = &sysclk_mutex,
};

static struct clk sysclk2 = {
	.name = "sysclk2",
	.ops = &sysclk_ops,
	.cg_sel = AB8500_SYSULPCLKCTRL1_SYSCLKBUF2REQ,
	.mutex = &sysclk_mutex,
};

static struct clk sysclk3 = {
	.name = "sysclk3",
	.ops = &sysclk_ops,
	.cg_sel = AB8500_SYSULPCLKCTRL1_SYSCLKBUF3REQ,
	.mutex = &sysclk_mutex,
};

static struct clk sysclk4 = {
	.name = "sysclk4",
	.ops = &sysclk_ops,
	.cg_sel = AB8500_SYSULPCLKCTRL1_SYSCLKBUF4REQ,
	.mutex = &sysclk_mutex,
};

static struct clk rtc32k = {
	.name = "rtc32k",
	.rate = 32768,
};

static struct clk clkout0 = {
	.name = "clkout0",
	.ops = &clkout0_ops,
	.parent = &sysclk,
	.rate = 9600000,
	.mutex = &sysclk_mutex,
};

static struct clk clkout1 = {
	.name = "clkout1",
	.ops = &clkout1_ops,
	.parent = &sysclk,
	.rate = 9600000,
	.mutex = &sysclk_mutex,
};

static struct clk ab_ulpclk = {
	.name = "ab_ulpclk",
	.ops = &ab_ulpclk_ops,
	.rate = 38400000,
	.mutex = &ab_ulpclk_mutex,
};

static struct clk *audioclk_parents[] = { &sysclk, &ab_ulpclk, NULL };

static struct clk audioclk = {
	.name = "audioclk",
	.ops = &audioclk_ops,
	.mutex = &audioclk_mutex,
	.parent = &sysclk,
	.parents = audioclk_parents,
};

static DEF_PRCMU_CLK(sgaclk, PRCMU_SGACLK, 320000000);
static DEF_PRCMU_CLK(uartclk, PRCMU_UARTCLK, 36360000);
static DEF_PRCMU_CLK(msp02clk, PRCMU_MSP02CLK, 19200000);
static DEF_PRCMU_CLK(msp1clk, PRCMU_MSP1CLK, 19200000);
static DEF_PRCMU_CLK(i2cclk, PRCMU_I2CCLK, 24000000);
static DEF_PRCMU_CLK(slimclk, PRCMU_SLIMCLK, 19200000);
static DEF_PRCMU_CLK(per1clk, PRCMU_PER1CLK, 133330000);
static DEF_PRCMU_CLK(per2clk, PRCMU_PER2CLK, 133330000);
static DEF_PRCMU_CLK(per3clk, PRCMU_PER3CLK, 133330000);
static DEF_PRCMU_CLK(per5clk, PRCMU_PER5CLK, 133330000);
static DEF_PRCMU_CLK(per6clk, PRCMU_PER6CLK, 133330000);
static DEF_PRCMU_CLK(per7clk, PRCMU_PER7CLK, 100000000);
static DEF_PRCMU_CLK(lcdclk, PRCMU_LCDCLK, 48000000);
static DEF_PRCMU_OPP100_CLK(bmlclk, PRCMU_BMLCLK, 200000000);
static DEF_PRCMU_CLK(hsitxclk, PRCMU_HSITXCLK, 100000000);
static DEF_PRCMU_CLK(hsirxclk, PRCMU_HSIRXCLK, 200000000);
static DEF_PRCMU_CLK(hdmiclk, PRCMU_HDMICLK, 76800000);
static DEF_PRCMU_CLK(apeatclk, PRCMU_APEATCLK, 160000000);
static DEF_PRCMU_CLK(apetraceclk, PRCMU_APETRACECLK, 160000000);
static DEF_PRCMU_CLK(mcdeclk, PRCMU_MCDECLK, 160000000);
static DEF_PRCMU_OPP100_CLK(ipi2cclk, PRCMU_IPI2CCLK, 24000000);
static DEF_PRCMU_CLK(dsialtclk, PRCMU_DSIALTCLK, 384000000);
static DEF_PRCMU_CLK(dmaclk, PRCMU_DMACLK, 200000000);
static DEF_PRCMU_CLK(b2r2clk, PRCMU_B2R2CLK, 200000000);
static DEF_PRCMU_CLK(tvclk, PRCMU_TVCLK, 76800000);
/* TODO: For SSPCLK, the spec says 24MHz, while the old driver says 48MHz. */
static DEF_PRCMU_CLK(sspclk, PRCMU_SSPCLK, 24000000);
static DEF_PRCMU_CLK(rngclk, PRCMU_RNGCLK, 19200000);
static DEF_PRCMU_CLK(uiccclk, PRCMU_UICCCLK, 48000000);
static DEF_PRCMU_CLK(timclk, PRCMU_TIMCLK, 3250000);
static DEF_PRCMU_CLK(sdmmcclk, PRCMU_SDMMCCLK, 50000000);

/* PRCC PClocks */

static DEF_PER1_PCLK(0, p1_pclk0);
static DEF_PER1_PCLK(1, p1_pclk1);
static DEF_PER1_PCLK(2, p1_pclk2);
static DEF_PER1_PCLK(3, p1_pclk3);
static DEF_PER1_PCLK(4, p1_pclk4);
static DEF_PER1_PCLK(5, p1_pclk5);
static DEF_PER1_PCLK(6, p1_pclk6);
static DEF_PER1_PCLK(7, p1_pclk7);
static DEF_PER1_PCLK(8, p1_pclk8);
static DEF_PER1_PCLK(9, p1_pclk9);
static DEF_PER1_PCLK(10, p1_pclk10);
static DEF_PER1_PCLK(11, p1_pclk11);

static DEF_PER2_PCLK(0, p2_pclk0);
static DEF_PER2_PCLK(1, p2_pclk1);
static DEF_PER2_PCLK(2, p2_pclk2);
static DEF_PER2_PCLK(3, p2_pclk3);
static DEF_PER2_PCLK(4, p2_pclk4);
static DEF_PER2_PCLK(5, p2_pclk5);
static DEF_PER2_PCLK(6, p2_pclk6);
static DEF_PER2_PCLK(7, p2_pclk7);
static DEF_PER2_PCLK(8, p2_pclk8);
static DEF_PER2_PCLK(9, p2_pclk9);
static DEF_PER2_PCLK(10, p2_pclk10);
static DEF_PER2_PCLK(11, p2_pclk11);
static DEF_PER2_PCLK(12, p2_pclk12);

static DEF_PER3_PCLK(0, p3_pclk0);
static DEF_PER3_PCLK(1, p3_pclk1);
static DEF_PER3_PCLK(2, p3_pclk2);
static DEF_PER3_PCLK(3, p3_pclk3);
static DEF_PER3_PCLK(4, p3_pclk4);
static DEF_PER3_PCLK(5, p3_pclk5);
static DEF_PER3_PCLK(6, p3_pclk6);
static DEF_PER3_PCLK(7, p3_pclk7);
static DEF_PER3_PCLK(8, p3_pclk8);

static DEF_PER5_PCLK(0, p5_pclk0);
static DEF_PER5_PCLK(1, p5_pclk1);

static DEF_PER6_PCLK(0, p6_pclk0);
static DEF_PER6_PCLK(1, p6_pclk1);
static DEF_PER6_PCLK(2, p6_pclk2);
static DEF_PER6_PCLK(3, p6_pclk3);
static DEF_PER6_PCLK(4, p6_pclk4);
static DEF_PER6_PCLK(5, p6_pclk5);
static DEF_PER6_PCLK(6, p6_pclk6);
static DEF_PER6_PCLK(7, p6_pclk7);

static DEF_PER7_PCLK(0, p7_pclk0);
static DEF_PER7_PCLK(1, p7_pclk1);
static DEF_PER7_PCLK(2, p7_pclk2);
static DEF_PER7_PCLK(3, p7_pclk3);
static DEF_PER7_PCLK(4, p7_pclk4);

/* UART0 */
static DEF_PER1_KCLK(0, p1_uart0_kclk, &uartclk);
static DEF_PER_CLK(p1_uart0_clk, &p1_pclk0, &p1_uart0_kclk);

/* UART1 */
static DEF_PER1_KCLK(1, p1_uart1_kclk, &uartclk);
static DEF_PER_CLK(p1_uart1_clk, &p1_pclk1, &p1_uart1_kclk);

/* I2C1 */
static DEF_PER1_KCLK(2, p1_i2c1_kclk, &i2cclk);
static DEF_PER_CLK(p1_i2c1_clk, &p1_pclk2, &p1_i2c1_kclk);

/* MSP0 */
static DEF_PER1_KCLK(3, p1_msp0_kclk, &msp02clk);
static DEF_PER_CLK(p1_msp0_clk, &p1_pclk3, &p1_msp0_kclk);

/* MSP1 */
static DEF_PER1_KCLK(4, p1_msp1_kclk, &msp1clk);
static DEF_PER_CLK(p1_msp1_clk, &p1_pclk4, &p1_msp1_kclk);

static DEF_PER1_KCLK(4, p1_msp1_ed_kclk, &msp02clk);
static DEF_PER_CLK(p1_msp1_ed_clk, &p1_pclk4, &p1_msp1_ed_kclk);

/* SDI0 */
static DEF_PER1_KCLK(5, p1_sdi0_kclk, &sdmmcclk);
static DEF_PER_CLK(p1_sdi0_clk, &p1_pclk5, &p1_sdi0_kclk);

/* I2C2 */
static DEF_PER1_KCLK(6, p1_i2c2_kclk, &i2cclk);
static DEF_PER_CLK(p1_i2c2_clk, &p1_pclk6, &p1_i2c2_kclk);

/* SLIMBUS0 */
static DEF_PER1_KCLK(3, p1_slimbus0_kclk, &slimclk);
static DEF_PER_CLK(p1_slimbus0_clk, &p1_pclk8, &p1_slimbus0_kclk);

/* I2C4 */
static DEF_PER1_KCLK(9, p1_i2c4_kclk, &i2cclk);
static DEF_PER_CLK(p1_i2c4_clk, &p1_pclk10, &p1_i2c4_kclk);

/* MSP3 */
static DEF_PER1_KCLK(10, p1_msp3_kclk, &msp1clk);
static DEF_PER_CLK(p1_msp3_clk, &p1_pclk11, &p1_msp3_kclk);

/* I2C3 */
static DEF_PER2_KCLK(0, p2_i2c3_kclk, &i2cclk);
static DEF_PER_CLK(p2_i2c3_clk, &p2_pclk0, &p2_i2c3_kclk);

/* SDI4 */
static DEF_PER2_KCLK(2, p2_sdi4_kclk, &sdmmcclk);
static DEF_PER_CLK(p2_sdi4_clk, &p2_pclk4, &p2_sdi4_kclk);

/* MSP2 */
static DEF_PER2_KCLK(3, p2_msp2_kclk, &msp02clk);
static DEF_PER_CLK(p2_msp2_clk, &p2_pclk5, &p2_msp2_kclk);

static DEF_PER2_KCLK(4, p2_msp2_ed_kclk, &msp02clk);
static DEF_PER_CLK(p2_msp2_ed_clk, &p2_pclk6, &p2_msp2_ed_kclk);

/* SDI1 */
static DEF_PER2_KCLK(4, p2_sdi1_kclk, &sdmmcclk);
static DEF_PER_CLK(p2_sdi1_clk, &p2_pclk6, &p2_sdi1_kclk);

/* These are probably broken now. */
static DEF_PER2_KCLK(5, p2_sdi1_ed_kclk, &sdmmcclk);
static DEF_PER_CLK(p2_sdi1_ed_clk, &p2_pclk7, &p2_sdi1_ed_kclk);

/* SDI3 */
static DEF_PER2_KCLK(5, p2_sdi3_kclk, &sdmmcclk);
static DEF_PER_CLK(p2_sdi3_clk, &p2_pclk7, &p2_sdi3_kclk);

/* These are probably broken now. */
static DEF_PER2_KCLK(6, p2_sdi3_ed_kclk, &sdmmcclk);
static DEF_PER_CLK(p2_sdi3_ed_clk, &p2_pclk8, &p2_sdi3_ed_kclk);

/* SSP0 */
static DEF_PER3_KCLK(1, p3_ssp0_kclk, &sspclk);
static DEF_PER_CLK(p3_ssp0_clk, &p3_pclk1, &p3_ssp0_kclk);

static DEF_PER3_KCLK(1, p3_ssp0_ed_kclk, &i2cclk);
static DEF_PER_CLK(p3_ssp0_ed_clk, &p3_pclk1, &p3_ssp0_ed_kclk);

/* SSP1 */
static DEF_PER3_KCLK(2, p3_ssp1_kclk, &sspclk);
static DEF_PER_CLK(p3_ssp1_clk, &p3_pclk2, &p3_ssp1_kclk);

static DEF_PER3_KCLK(2, p3_ssp1_ed_kclk, &i2cclk);
static DEF_PER_CLK(p3_ssp1_ed_clk, &p3_pclk2, &p3_ssp1_ed_kclk);

/* I2C0 */
static DEF_PER3_KCLK(3, p3_i2c0_kclk, &i2cclk);
static DEF_PER_CLK(p3_i2c0_clk, &p3_pclk3, &p3_i2c0_kclk);

/* SDI2 */
static DEF_PER3_KCLK(4, p3_sdi2_kclk, &sdmmcclk);
static DEF_PER_CLK(p3_sdi2_clk, &p3_pclk4, &p3_sdi2_kclk);

/* SKE */
static DEF_PER3_KCLK(5, p3_ske_kclk, &rtc32k);
static DEF_PER_CLK(p3_ske_clk, &p3_pclk5, &p3_ske_kclk);

/* UART2 */
static DEF_PER3_KCLK(6, p3_uart2_kclk, &uartclk);
static DEF_PER_CLK(p3_uart2_clk, &p3_pclk6, &p3_uart2_kclk);

/* SDI5 */
static DEF_PER3_KCLK(7, p3_sdi5_kclk, &sdmmcclk);
static DEF_PER_CLK(p3_sdi5_clk, &p3_pclk7, &p3_sdi5_kclk);

/* USB */
static DEF_PER5_KCLK(0, p5_usb_ed_kclk, &i2cclk);
static DEF_PER_CLK(p5_usb_ed_clk, &p5_pclk0, &p5_usb_ed_kclk);

/* RNG */
static DEF_PER6_KCLK(0, p6_rng_kclk, &rngclk);
static DEF_PER_CLK(p6_rng_clk, &p6_pclk0, &p6_rng_kclk);

static DEF_PER6_KCLK(0, p6_rng_ed_kclk, &i2cclk);
static DEF_PER_CLK(p6_rng_ed_clk, &p6_pclk0, &p6_rng_ed_kclk);


/* MTU:S */

/* MTU0 */
static DEF_PER_CLK(p6_mtu0_clk, &p6_pclk6, &timclk);
static DEF_PER_CLK(p7_mtu0_ed_clk, &p7_pclk2, &timclk);

/* MTU1 */
static DEF_PER_CLK(p6_mtu1_clk, &p6_pclk7, &timclk);
static DEF_PER_CLK(p7_mtu1_ed_clk, &p7_pclk3, &timclk);

static struct clk *db5500_dbg_clks[] = {
	/* Clock sources */
	&soc0_pll,
	&soc1_pll,
	&ddr_pll,
	&ulp38m4,
	&sysclk,
	&rtc32k,
	/* PRCMU clocks */
	&sgaclk,
	&uartclk,
	&msp02clk,
	&msp1clk,
	&i2cclk,
	&sdmmcclk,
	&slimclk,
	&per1clk,
	&per2clk,
	&per3clk,
	&per5clk,
	&per6clk,
	&per7clk,
	&lcdclk,
	&bmlclk,
	&hsitxclk,
	&hsirxclk,
	&hdmiclk,
	&apeatclk,
	&apetraceclk,
	&mcdeclk,
	&ipi2cclk,
	&dsialtclk,
	&dmaclk,
	&b2r2clk,
	&tvclk,
	&sspclk,
	&rngclk,
	&uiccclk,
	/* Clock sources */
	&sysclk2,
	&clkout0,
	&clkout1,
};

/*
 * TODO: Ensure names match with devices and then remove unnecessary entries
 * when all drivers use the clk API.
 */

#define CLK_LOOKUP(_clk, _dev_id, _con_id) \
	{ .dev_id = _dev_id, .con_id = _con_id, .clk = &_clk }

static struct clk_lookup u8500_common_clock_sources[] = {
	CLK_LOOKUP(soc0_pll, NULL, "soc0_pll"),
	CLK_LOOKUP(soc1_pll, NULL, "soc1_pll"),
	CLK_LOOKUP(ddr_pll, NULL, "ddr_pll"),
	CLK_LOOKUP(ulp38m4, NULL, "ulp38m4"),
	CLK_LOOKUP(sysclk, NULL, "sysclk"),
	CLK_LOOKUP(rtc32k, NULL, "clk32k"),
	CLK_LOOKUP(sysclk, "ab8500-usb.0", "sysclk"),
	CLK_LOOKUP(sysclk, "ab8500-codec.0", "sysclk"),
	CLK_LOOKUP(ab_ulpclk, "ab8500-codec.0", "ulpclk"),
	CLK_LOOKUP(audioclk, "ab8500-codec.0", "audioclk"),
};

static struct clk_lookup u8500_v2_sysclks[] = {
	CLK_LOOKUP(sysclk2, NULL, "sysclk2"),
	CLK_LOOKUP(sysclk3, NULL, "sysclk3"),
	CLK_LOOKUP(sysclk4, NULL, "sysclk4"),
};

static struct clk_lookup u8500_common_prcmu_clocks[] = {
	CLK_LOOKUP(sgaclk, "mali", NULL),
	CLK_LOOKUP(uartclk, "UART", NULL),
	CLK_LOOKUP(msp02clk, "MSP02", NULL),
	CLK_LOOKUP(i2cclk, "I2C", NULL),
	CLK_LOOKUP(sdmmcclk, "sdmmc", NULL),
	CLK_LOOKUP(slimclk, "slim", NULL),
	CLK_LOOKUP(per1clk, "PERIPH1", NULL),
	CLK_LOOKUP(per2clk, "PERIPH2", NULL),
	CLK_LOOKUP(per3clk, "PERIPH3", NULL),
	CLK_LOOKUP(per5clk, "PERIPH5", NULL),
	CLK_LOOKUP(per6clk, "PERIPH6", NULL),
	CLK_LOOKUP(per7clk, "PERIPH7", NULL),
	CLK_LOOKUP(lcdclk, "lcd", NULL),
	CLK_LOOKUP(bmlclk, "bml", NULL),
	CLK_LOOKUP(hsitxclk, "stm-hsi.0", NULL),
	CLK_LOOKUP(hsirxclk, "stm-hsi.1", NULL),
	CLK_LOOKUP(lcdclk, "mcde", "lcd"),
	CLK_LOOKUP(hdmiclk, "hdmi", NULL),
	CLK_LOOKUP(hdmiclk, "mcde", "hdmi"),
	CLK_LOOKUP(apeatclk, "apeat", NULL),
	CLK_LOOKUP(apetraceclk, "apetrace", NULL),
	CLK_LOOKUP(mcdeclk, "mcde", NULL),
	CLK_LOOKUP(mcdeclk, "mcde", "mcde"),
	CLK_LOOKUP(ipi2cclk, "ipi2", NULL),
	CLK_LOOKUP(dmaclk, "dma40.0", NULL),
	CLK_LOOKUP(b2r2clk, "b2r2", NULL),
	CLK_LOOKUP(b2r2clk, "b2r2_bus", NULL),
	CLK_LOOKUP(b2r2clk, "U8500-B2R2.0", NULL),
	CLK_LOOKUP(tvclk, "tv", NULL),
	CLK_LOOKUP(tvclk, "mcde", "tv"),
};

static struct clk_lookup u8500_common_prcc_clocks[] = {
	/* PERIPH 1 */
	CLK_LOOKUP(p1_uart0_clk, "uart0", NULL),
	CLK_LOOKUP(p1_uart1_clk, "uart1", NULL),
	CLK_LOOKUP(p1_i2c1_clk, "nmk-i2c.1", NULL),
	CLK_LOOKUP(p1_msp0_clk, "msp0", NULL),
	CLK_LOOKUP(p1_msp0_clk, "MSP_I2S.0", NULL),
	CLK_LOOKUP(p1_sdi0_clk, "sdi0", NULL),
	CLK_LOOKUP(p1_i2c2_clk, "nmk-i2c.2", NULL),
	CLK_LOOKUP(p1_slimbus0_clk, "slimbus0", NULL),
	CLK_LOOKUP(p1_pclk9, "gpio.0", NULL),
	CLK_LOOKUP(p1_pclk9, "gpio.1", NULL),
	CLK_LOOKUP(p1_pclk9, "gpioblock0", NULL),

	/* PERIPH 2 */
	CLK_LOOKUP(p2_i2c3_clk, "nmk-i2c.3", NULL),
	CLK_LOOKUP(p2_pclk1, "spi2", NULL),
	CLK_LOOKUP(p2_pclk2, "spi1", NULL),
	CLK_LOOKUP(p2_pclk3, "pwl", NULL),
	CLK_LOOKUP(p2_sdi4_clk, "sdi4", NULL),

	/* PERIPH 3 */
	CLK_LOOKUP(p3_pclk0, "fsmc", NULL),
	CLK_LOOKUP(p3_i2c0_clk, "nmk-i2c.0", NULL),
	CLK_LOOKUP(p3_sdi2_clk, "sdi2", NULL),
	CLK_LOOKUP(p3_ske_clk, "ske", NULL),
	CLK_LOOKUP(p3_ske_clk, "nmk-ske-keypad", NULL),
	CLK_LOOKUP(p3_uart2_clk, "uart2", NULL),
	CLK_LOOKUP(p3_sdi5_clk, "sdi5", NULL),
	CLK_LOOKUP(p3_pclk8, "gpio.2", NULL),
	CLK_LOOKUP(p3_pclk8, "gpio.3", NULL),
	CLK_LOOKUP(p3_pclk8, "gpio.4", NULL),
	CLK_LOOKUP(p3_pclk8, "gpio.5", NULL),
	CLK_LOOKUP(p3_pclk8, "gpioblock2", NULL),

	/* PERIPH 5 */
	CLK_LOOKUP(p5_pclk1, "gpio.8", NULL),
	CLK_LOOKUP(p5_pclk1, "gpioblock3", NULL),

	/* PERIPH 6 */
	CLK_LOOKUP(p6_pclk1, "cryp0", NULL),
	CLK_LOOKUP(p6_pclk2, "hash0", NULL),
	CLK_LOOKUP(p6_pclk3, "pka", NULL),
};

static struct clk_lookup u8500_ed_prcc_clocks[] = {
	/* PERIPH 1 */
	CLK_LOOKUP(p1_msp1_ed_clk, "msp1", NULL),
	CLK_LOOKUP(p1_msp1_ed_clk, "MSP_I2S.1", NULL),
	CLK_LOOKUP(p1_pclk7, "spi3", NULL),

	/* PERIPH 2 */
	CLK_LOOKUP(p2_msp2_ed_clk, "msp2", NULL),
	CLK_LOOKUP(p2_msp2_ed_clk, "MSP_I2S.2", NULL),
	CLK_LOOKUP(p2_sdi1_ed_clk, "sdi1", NULL),
	CLK_LOOKUP(p2_sdi3_ed_clk, "sdi3", NULL),
	CLK_LOOKUP(p2_pclk9, "spi0", NULL),
	CLK_LOOKUP(p2_pclk10, "ssirx", NULL),
	CLK_LOOKUP(p2_pclk11, "ssitx", NULL),
	CLK_LOOKUP(p2_pclk12, "gpio.6", NULL),
	CLK_LOOKUP(p2_pclk12, "gpio.7", NULL),
	CLK_LOOKUP(p2_pclk12, "gpioblock1", NULL),

	/* PERIPH 3 */
	CLK_LOOKUP(p3_ssp0_ed_clk, "ssp0", NULL),
	CLK_LOOKUP(p3_ssp1_ed_clk, "ssp1", NULL),

	/* PERIPH 5 */
	CLK_LOOKUP(p5_usb_ed_clk, "musb_hdrc.0", "usb"),

	/* PERIPH 6 */
	CLK_LOOKUP(p6_rng_ed_clk, "rng", NULL),
	CLK_LOOKUP(p6_pclk4, "cryp1", NULL),
	CLK_LOOKUP(p6_pclk5, "hash1", NULL),
	CLK_LOOKUP(p6_pclk6, "dmc", NULL),

	/* PERIPH 7 */
	CLK_LOOKUP(p7_pclk0, "cfgreg", NULL),
	CLK_LOOKUP(p7_pclk1, "wdg", NULL),
	CLK_LOOKUP(p7_mtu0_ed_clk, "mtu0", NULL),
	CLK_LOOKUP(p7_mtu1_ed_clk, "mtu1", NULL),
	CLK_LOOKUP(p7_pclk4, "tzpc0", NULL),
};

static struct clk_lookup u8500_v1_v2_prcmu_clocks[] = {
	CLK_LOOKUP(msp1clk, "MSP1", NULL),
	CLK_LOOKUP(dsialtclk, "dsialt", NULL),
	CLK_LOOKUP(sspclk, "SSP", NULL),
	CLK_LOOKUP(rngclk, "rngclk", NULL),
	CLK_LOOKUP(uiccclk, "uicc", NULL),
};

static struct clk_lookup u8500_v1_v2_prcc_clocks[] = {
	/* PERIPH 1 */
	CLK_LOOKUP(p1_msp1_clk, "msp1", NULL),
	CLK_LOOKUP(p1_msp1_clk, "MSP_I2S.1", NULL),
	CLK_LOOKUP(p1_pclk7, "spi3", NULL),
	CLK_LOOKUP(p1_i2c4_clk, "nmk-i2c.4", NULL),

	/* PERIPH 2 */
	CLK_LOOKUP(p2_msp2_clk, "msp2", NULL),
	CLK_LOOKUP(p2_msp2_clk, "MSP_I2S.2", NULL),
	CLK_LOOKUP(p2_sdi1_clk, "sdi1", NULL),
	CLK_LOOKUP(p2_sdi3_clk, "sdi3", NULL),
	CLK_LOOKUP(p2_pclk8, "spi0", NULL),
	CLK_LOOKUP(p2_pclk9, "ssirx", NULL),
	CLK_LOOKUP(p2_pclk10, "ssitx", NULL),
	CLK_LOOKUP(p2_pclk11, "gpio.6", NULL),
	CLK_LOOKUP(p2_pclk11, "gpio.7", NULL),
	CLK_LOOKUP(p2_pclk11, "gpioblock1", NULL),

	/* PERIPH 3 */
	CLK_LOOKUP(p3_ssp0_clk, "ssp0", NULL),
	CLK_LOOKUP(p3_ssp1_clk, "ssp1", NULL),

	/* PERIPH 5 */
	CLK_LOOKUP(p5_pclk0, "musb_hdrc.0", "usb"),

	/* PERIPH 6 */
	CLK_LOOKUP(p6_pclk5, "hash1", NULL),
	CLK_LOOKUP(p6_pclk4, "cryp1", NULL),
	CLK_LOOKUP(p6_rng_clk, "rng", NULL),
};

static struct clk_lookup u8500_v2_prcmu_clocks[] = {
	CLK_LOOKUP(clkout0, "pri-cam", NULL),
	CLK_LOOKUP(clkout1, "3-005c", NULL),
	CLK_LOOKUP(clkout1, "3-005d", NULL),
	CLK_LOOKUP(clkout1, "sec-cam", NULL),
};

static struct clk_lookup u8500_v2_prcc_clocks[] = {
	/* PERIPH 1 */
	CLK_LOOKUP(p1_msp3_clk, "msp3", NULL),
	CLK_LOOKUP(p1_msp3_clk, "MSP_I2S.3", NULL),

	/* PERIPH 6 */
	CLK_LOOKUP(p6_pclk4, "hash1", NULL),
	CLK_LOOKUP(p6_pclk4, "cryp1", NULL),
	CLK_LOOKUP(p6_pclk5, "cfgreg", NULL),
	CLK_LOOKUP(p6_mtu0_clk, "mtu0", NULL),
	CLK_LOOKUP(p6_mtu1_clk, "mtu1", NULL),
};

/* these are the clocks which are default from the bootloader */
static const char *u8500_boot_clk[] = {
	"uart0",
	"uart1",
	"uart2",
	"gpioblock0",
	"gpioblock1",
	"gpioblock2",
	"gpioblock3",
	"mtu0",
	"mtu1",
	"ssp0",
	"ssp1",
	"spi0",
	"spi1",
	"spi2",
	"spi3",
	"msp0",
	"msp1",
	"msp2",
	"nmk-i2c.0",
	"nmk-i2c.1",
	"nmk-i2c.2",
	"nmk-i2c.3",
	"nmk-i2c.4",
};

static void sysclk_init_disable(struct work_struct *not_used)
{
	int i;

	mutex_lock(&sysclk_mutex);

	/* Enable SWAT  */
	if (ab8500_sysctrl_set(AB8500_SWATCTRL, AB8500_SWATCTRL_SWATENABLE))
		goto err_swat;

	for (i = 0; i < ARRAY_SIZE(u8500_v2_sysclks); i++) {
		struct clk *clk = u8500_v2_sysclks[i].clk;

		/* Disable sysclks */
		if (!clk->enabled && clk->cg_sel) {
			if (ab8500_sysctrl_clear(AB8500_SYSULPCLKCTRL1,
				(u8)clk->cg_sel))
				goto err_sysclk;
		}
	}
	goto unlock_and_exit;

err_sysclk:
	pr_err("clock: Disable %s failed", u8500_v2_sysclks[i].clk->name);
	ab8500_sysctrl_clear(AB8500_SWATCTRL, AB8500_SWATCTRL_SWATENABLE);
	goto unlock_and_exit;

err_swat:
	pr_err("clock: Enable SWAT failed");

unlock_and_exit:
	mutex_unlock(&sysclk_mutex);
}

struct clk *boot_clks[ARRAY_SIZE(u8500_boot_clk)];

/* we disable a majority of peripherals enabled by default
 * but without drivers
 */
static int __init u8500_boot_clk_disable(void)
{
	unsigned int i = 0;

	for (i = 0; i < ARRAY_SIZE(u8500_boot_clk); i++) {
		if (!boot_clks[i])
			continue;

		clk_disable(boot_clks[i]);
		clk_put(boot_clks[i]);
	}

	INIT_DELAYED_WORK(&sysclk_disable_work, sysclk_init_disable);
	schedule_delayed_work(&sysclk_disable_work, 10 * HZ);

	return 0;
}
late_initcall_sync(u8500_boot_clk_disable);

static void u8500_amba_clk_enable(void)
{
	unsigned int i = 0;

	writel(~0x0  & ~(1 << 9), __io_address(U8500_PER1_BASE + 0xF000
					       + 0x04));
	writel(~0x0, __io_address(U8500_PER1_BASE + 0xF000 + 0x0C));

	writel(~0x0 & ~(1 << 11), __io_address(U8500_PER2_BASE + 0xF000
					       + 0x04));
	writel(~0x0, __io_address(U8500_PER2_BASE + 0xF000 + 0x0C));

	/*GPIO,UART2 are enabled for booting*/
	writel(0xBF, __io_address(U8500_PER3_BASE + 0xF000 + 0x04));
	writel(~0x0 & ~(1 << 6), __io_address(U8500_PER3_BASE + 0xF000
					      + 0x0C));

	for (i = 0; i < ARRAY_SIZE(u8500_boot_clk); i++) {
		boot_clks[i] = clk_get_sys(u8500_boot_clk[i], NULL);
		clk_enable(boot_clks[i]);
	}
}

int __init db5500_clk_init(void)
{
	if (cpu_is_u5500() || ux500_is_svp()) {
		sysclk_ops.enable = NULL;
		sysclk_ops.disable = NULL;
		prcmu_clk_ops.enable = NULL;
		prcmu_clk_ops.disable = NULL;
		prcmu_opp100_clk_ops.enable = NULL;
		prcmu_opp100_clk_ops.disable = NULL;
		prcc_pclk_ops.enable = NULL;
		prcc_pclk_ops.disable = NULL;
		prcc_kclk_ops.enable = NULL;
		prcc_kclk_ops.disable = NULL;
		clkout0_ops.enable = NULL;
		clkout0_ops.disable = NULL;
		clkout1_ops.enable = NULL;
		clkout1_ops.disable = NULL;
	}

	clks_register(u8500_common_clock_sources,
		ARRAY_SIZE(u8500_common_clock_sources));
	clks_register(u8500_common_prcmu_clocks,
		ARRAY_SIZE(u8500_common_prcmu_clocks));
	clks_register(u8500_common_prcc_clocks,
		ARRAY_SIZE(u8500_common_prcc_clocks));

	if (cpu_is_u5500()) {
		clks_register(u8500_ed_prcc_clocks,
			ARRAY_SIZE(u8500_ed_prcc_clocks));
	} else if (cpu_is_u8500v2()) {
		clks_register(u8500_v2_sysclks,
			ARRAY_SIZE(u8500_v2_sysclks));
		clks_register(u8500_v1_v2_prcmu_clocks,
			ARRAY_SIZE(u8500_v1_v2_prcmu_clocks));
		clks_register(u8500_v2_prcmu_clocks,
			ARRAY_SIZE(u8500_v2_prcmu_clocks));
		clks_register(u8500_v1_v2_prcc_clocks,
			ARRAY_SIZE(u8500_v1_v2_prcc_clocks));
		clks_register(u8500_v2_prcc_clocks,
			ARRAY_SIZE(u8500_v2_prcc_clocks));
	}

	if (cpu_is_u8500())
		u8500_amba_clk_enable();

	/*
	 * The following clks are shared with secure world.
	 * Currently this leads to a limitation where we need to
	 * enable them at all times.
	 */
	clk_enable(&p6_pclk1);
	clk_enable(&p6_pclk2);
	clk_enable(&p6_pclk3);
	if (cpu_is_u8500() && !ux500_is_svp())
		clk_enable(&p6_rng_clk);

	writel(PRCM_TCR_DOZE_MODE | PRCM_TCR_TENSEL_MASK, _PRCMU_BASE + PRCM_TCR);

	return 0;
}

static int __init db5500_clk_debug_init(void)
{
	return dbx500_clk_debug_init(db5500_dbg_clks,
				     ARRAY_SIZE(db5500_dbg_clks));
}
module_init(db5500_clk_debug_init);
