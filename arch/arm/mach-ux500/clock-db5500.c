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
#include <linux/gpio/nomadik.h>
#include <linux/mfd/ab8500/sysctrl.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/abx500.h>

#include <plat/pincfg.h>

#include <mach/hardware.h>
#include <mach/prcmu.h>
#include <mach/prcmu-regs.h>

#include "clock.h"
#include "pins-db5500.h"

static DEFINE_MUTEX(sysclk_mutex);
static DEFINE_MUTEX(pll_mutex);
static DEFINE_MUTEX(ab_ulpclk_mutex);
static DEFINE_MUTEX(audioclk_mutex);

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

static int rtc_clk_enable(struct clk *clk)
{
	return ab5500_clock_rtc_enable(clk->cg_sel, true);
}

static void rtc_clk_disable(struct clk *clk)
{
	int ret = ab5500_clock_rtc_enable(clk->cg_sel, false);

	if (ret)
		pr_err("clock: %s failed to disable: %d\n", clk->name, ret);
}

static struct clkops rtc_clk_ops = {
	.enable		= rtc_clk_enable,
	.disable	= rtc_clk_disable,
};

static pin_cfg_t clkout0_pins[] = {
	GPIO161_CLKOUT_0 | PIN_OUTPUT_LOW,
};

static pin_cfg_t clkout1_pins[] = {
	GPIO162_CLKOUT_1 | PIN_OUTPUT_LOW,
};

static int clkout0_enable(struct clk *clk)
{
	unsigned int val = readl(PRCM_CLKOCR);

	val &= ~PRCM_CLKOCR_CLKOUT0_MASK;
	val |= PRCM_CLKOCR_CLKOUT0_REF_CLK;

	writel(val, PRCM_CLKOCR);

	return nmk_config_pins(clkout0_pins, ARRAY_SIZE(clkout0_pins));
}

static void clkout0_disable(struct clk *clk)
{
	int r;

	r = nmk_config_pins_sleep(clkout0_pins, ARRAY_SIZE(clkout0_pins));
	if (!r)
		return;

	pr_err("clock: failed to disable %s.\n", clk->name);
}

static int clkout1_enable(struct clk *clk)
{
	unsigned int val = readl(PRCM_CLKOCR);

	val &= ~PRCM_CLKOCR_CLKOUT1_MASK;
	val |= PRCM_CLKOCR_CLKOUT1_REF_CLK;

	writel(val, PRCM_CLKOCR);

	return nmk_config_pins(clkout1_pins, ARRAY_SIZE(clkout0_pins));
}

static void clkout1_disable(struct clk *clk)
{
	int r;

	r = nmk_config_pins_sleep(clkout1_pins, ARRAY_SIZE(clkout1_pins));
	if (!r)
		return;

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
	DEF_PRCC_PCLK(_name, U5500_CLKRST1_BASE, _cg_bit, &per1clk)
#define DEF_PER2_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U5500_CLKRST2_BASE, _cg_bit, &per2clk)
#define DEF_PER3_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U5500_CLKRST3_BASE, _cg_bit, &per3clk)
#define DEF_PER5_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U5500_CLKRST5_BASE, _cg_bit, &per5clk)
#define DEF_PER6_PCLK(_cg_bit, _name) \
	DEF_PRCC_PCLK(_name, U5500_CLKRST6_BASE, _cg_bit, &per6clk)

#define DEF_PER1_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U5500_CLKRST1_BASE, _cg_bit, _parent)
#define DEF_PER2_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U5500_CLKRST2_BASE, _cg_bit, _parent)
#define DEF_PER3_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U5500_CLKRST3_BASE, _cg_bit, _parent)
#define DEF_PER5_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U5500_CLKRST5_BASE, _cg_bit, _parent)
#define DEF_PER6_KCLK(_cg_bit, _name, _parent) \
	DEF_PRCC_KCLK(_name, U5500_CLKRST6_BASE, _cg_bit, _parent)

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
	.ops = &prcmu_clk_ops,
	.mutex = &pll_mutex,
	.cg_sel = PRCMU_PLLSOC0,
};

static struct clk soc1_pll = {
	.name = "soc1_pll",
	.ops = &prcmu_clk_ops,
	.mutex = &pll_mutex,
	.cg_sel = PRCMU_PLLSOC1,
};

static struct clk ddr_pll = {
	.name = "ddr_pll",
	.ops = &prcmu_clk_ops,
	.mutex = &pll_mutex,
	.cg_sel = PRCMU_PLLDDR,
};

static struct clk ulp38m4 = {
	.name = "ulp38m4",
};

static struct clk sysclk = {
	.name = "sysclk",
	.ops = &sysclk_ops,
	.rate = 26000000,
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

static struct clk kbd32k = {
	.name = "kbd32k",
	.rate = 32768,
};

static struct clk clk_dummy = {
	.name = "dummy",
};

static struct clk clk_msp1 = {
	.name = "msp1",
	.rate = 26000000,
};

static struct clk rtc_clk1 = {
	.name	= "rtc_clk1",
	.ops	= &rtc_clk_ops,
	.cg_sel	= 1,
	.mutex = &sysclk_mutex,
};

static struct clk clkout0 = {
	.name = "clkout0",
	.ops = &clkout0_ops,
	.parent = &sysclk,
	.mutex = &sysclk_mutex,
};

static struct clk clkout1 = {
	.name = "clkout1",
	.ops = &clkout1_ops,
	.parent = &sysclk,
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

static DEF_PRCMU_CLK(dmaclk, PRCMU_DMACLK, 200000000);
static DEF_PRCMU_CLK(b2r2clk, PRCMU_B2R2CLK, 200000000);
static DEF_PRCMU_CLK(sgaclk, PRCMU_SGACLK, 199900000);
static DEF_PRCMU_CLK(uartclk, PRCMU_UARTCLK, 36360000);
static DEF_PRCMU_CLK(msp02clk, PRCMU_MSP02CLK, 13000000);
static DEF_PRCMU_CLK(i2cclk, PRCMU_I2CCLK, 24000000);
static DEF_PRCMU_CLK(irdaclk, PRCMU_IRDACLK, 48000000);
static DEF_PRCMU_CLK(irrcclk, PRCMU_IRRCCLK, 48000000);
static DEF_PRCMU_CLK(rngclk, PRCMU_RNGCLK, 26000000);
static DEF_PRCMU_CLK(pwmclk, PRCMU_PWMCLK, 26000000);
static DEF_PRCMU_CLK(sdmmcclk, PRCMU_SDMMCCLK, 100000000);
static DEF_PRCMU_CLK(per1clk, PRCMU_PER1CLK, 133330000);
static DEF_PRCMU_CLK(per2clk, PRCMU_PER2CLK, 133330000);
static DEF_PRCMU_CLK(per3clk, PRCMU_PER3CLK, 133330000);
static DEF_PRCMU_CLK(per5clk, PRCMU_PER5CLK, 133330000);
static DEF_PRCMU_CLK(per6clk, PRCMU_PER6CLK, 133330000);
static DEF_PRCMU_CLK(hdmiclk, PRCMU_HDMICLK, 26000000);
static DEF_PRCMU_CLK(apeatclk, PRCMU_APEATCLK, 200000000);
static DEF_PRCMU_CLK(apetraceclk, PRCMU_APETRACECLK, 266000000);
static DEF_PRCMU_CLK(mcdeclk, PRCMU_MCDECLK, 160000000);
static DEF_PRCMU_CLK(tvclk, PRCMU_TVCLK, 40000000);
static DEF_PRCMU_CLK(dsialtclk, PRCMU_DSIALTCLK, 400000000);
static DEF_PRCMU_CLK(timclk, PRCMU_TIMCLK, 3250000);
static DEF_PRCMU_CLK(svaclk, PRCMU_SVACLK, 156000000);
static DEF_PRCMU_CLK(siaclk, PRCMU_SIACLK, 133330000);

/* PRCC PClocks */

static DEF_PER1_PCLK(0, p1_pclk0);
static DEF_PER1_PCLK(1, p1_pclk1);
static DEF_PER1_PCLK(2, p1_pclk2);
static DEF_PER1_PCLK(3, p1_pclk3);
static DEF_PER1_PCLK(4, p1_pclk4);
static DEF_PER1_PCLK(5, p1_pclk5);
static DEF_PER1_PCLK(6, p1_pclk6);

static DEF_PER2_PCLK(0, p2_pclk0);
static DEF_PER2_PCLK(1, p2_pclk1);

static DEF_PER3_PCLK(0, p3_pclk0);
static DEF_PER3_PCLK(1, p3_pclk1);
static DEF_PER3_PCLK(2, p3_pclk2);

static DEF_PER5_PCLK(0, p5_pclk0);
static DEF_PER5_PCLK(1, p5_pclk1);
static DEF_PER5_PCLK(2, p5_pclk2);
static DEF_PER5_PCLK(3, p5_pclk3);
static DEF_PER5_PCLK(4, p5_pclk4);
static DEF_PER5_PCLK(5, p5_pclk5);
static DEF_PER5_PCLK(6, p5_pclk6);
static DEF_PER5_PCLK(7, p5_pclk7);
static DEF_PER5_PCLK(8, p5_pclk8);
static DEF_PER5_PCLK(9, p5_pclk9);
static DEF_PER5_PCLK(10, p5_pclk10);
static DEF_PER5_PCLK(11, p5_pclk11);
static DEF_PER5_PCLK(12, p5_pclk12);
static DEF_PER5_PCLK(13, p5_pclk13);
static DEF_PER5_PCLK(14, p5_pclk14);
static DEF_PER5_PCLK(15, p5_pclk15);

static DEF_PER6_PCLK(0, p6_pclk0);
static DEF_PER6_PCLK(1, p6_pclk1);
static DEF_PER6_PCLK(2, p6_pclk2);
static DEF_PER6_PCLK(3, p6_pclk3);
static DEF_PER6_PCLK(4, p6_pclk4);
static DEF_PER6_PCLK(5, p6_pclk5);
static DEF_PER6_PCLK(6, p6_pclk6);
static DEF_PER6_PCLK(7, p6_pclk7);

/* MSP0 */
static DEF_PER1_KCLK(0, p1_msp0_kclk, &msp02clk);
static DEF_PER_CLK(p1_msp0_clk, &p1_pclk0, &p1_msp0_kclk);

/* SDI0 */
static DEF_PER1_KCLK(1, p1_sdi0_kclk, &sdmmcclk);
static DEF_PER_CLK(p1_sdi0_clk, &p1_pclk1, &p1_sdi0_kclk);

/* SDI2 */
static DEF_PER1_KCLK(2, p1_sdi2_kclk, &sdmmcclk);
static DEF_PER_CLK(p1_sdi2_clk, &p1_pclk2, &p1_sdi2_kclk);

/* UART0 */
static DEF_PER1_KCLK(3, p1_uart0_kclk, &uartclk);
static DEF_PER_CLK(p1_uart0_clk, &p1_pclk3, &p1_uart0_kclk);

/* I2C1 */
static DEF_PER1_KCLK(4, p1_i2c1_kclk, &i2cclk);
static DEF_PER_CLK(p1_i2c1_clk, &p1_pclk4, &p1_i2c1_kclk);

/* PWM */
static DEF_PER3_KCLK(0, p3_pwm_kclk, &pwmclk);
static DEF_PER_CLK(p3_pwm_clk, &p3_pclk1, &p3_pwm_kclk);

/* KEYPAD */
static DEF_PER3_KCLK(0, p3_keypad_kclk, &kbd32k);
static DEF_PER_CLK(p3_keypad_clk, &p3_pclk0, &p3_keypad_kclk);

/* MSP2 */
static DEF_PER5_KCLK(0, p5_msp2_kclk, &msp02clk);
static DEF_PER_CLK(p5_msp2_clk, &p5_pclk0, &p5_msp2_kclk);

/* UART1 */
static DEF_PER5_KCLK(1, p5_uart1_kclk, &uartclk);
static DEF_PER_CLK(p5_uart1_clk, &p5_pclk1, &p5_uart1_kclk);

/* UART2 */
static DEF_PER5_KCLK(2, p5_uart2_kclk, &uartclk);
static DEF_PER_CLK(p5_uart2_clk, &p5_pclk2, &p5_uart2_kclk);

/* UART3 */
static DEF_PER5_KCLK(3, p5_uart3_kclk, &uartclk);
static DEF_PER_CLK(p5_uart3_clk, &p5_pclk3, &p5_uart3_kclk);

/* SDI1 */
static DEF_PER5_KCLK(4, p5_sdi1_kclk, &sdmmcclk);
static DEF_PER_CLK(p5_sdi1_clk, &p5_pclk4, &p5_sdi1_kclk);

/* SDI3 */
static DEF_PER5_KCLK(5, p5_sdi3_kclk, &sdmmcclk);
static DEF_PER_CLK(p5_sdi3_clk, &p5_pclk5, &p5_sdi3_kclk);

/* SDI4 */
static DEF_PER5_KCLK(6, p5_sdi4_kclk, &sdmmcclk);
static DEF_PER_CLK(p5_sdi4_clk, &p5_pclk6, &p5_sdi4_kclk);

/* I2C2 */
static DEF_PER5_KCLK(7, p5_i2c2_kclk, &i2cclk);
static DEF_PER_CLK(p5_i2c2_clk, &p5_pclk7, &p5_i2c2_kclk);

/* I2C3 */
static DEF_PER5_KCLK(8, p5_i2c3_kclk, &i2cclk);
static DEF_PER_CLK(p5_i2c3_clk, &p5_pclk8, &p5_i2c3_kclk);

/* IRRC */
static DEF_PER5_KCLK(9, p5_irrc_kclk, &irrcclk);
static DEF_PER_CLK(p5_irrc_clk, &p5_pclk9, &p5_irrc_kclk);

/* IRDA */
static DEF_PER5_KCLK(10, p5_irda_kclk, &irdaclk);
static DEF_PER_CLK(p5_irda_clk, &p5_pclk10, &p5_irda_kclk);

/* RNG */
static DEF_PER6_KCLK(0, p6_rng_kclk, &rngclk);
static DEF_PER_CLK(p6_rng_clk, &p6_pclk0, &p6_rng_kclk);

/* MTU:S */

/* MTU0 */
static DEF_PER_CLK(p6_mtu0_clk, &p6_pclk6, &timclk);

/* MTU1 */
static DEF_PER_CLK(p6_mtu1_clk, &p6_pclk7, &timclk);

static struct clk *db5500_dbg_clks[] __initdata = {
	/* Clock sources */
	&soc0_pll,
	&soc1_pll,
	&ddr_pll,
	&ulp38m4,
	&sysclk,
	&rtc32k,

	/* PRCMU clocks */
	&sgaclk,
	&siaclk,
	&svaclk,
	&uartclk,
	&msp02clk,
	&i2cclk,
	&irdaclk,
	&irrcclk,
	&sdmmcclk,
	&per1clk,
	&per2clk,
	&per3clk,
	&per5clk,
	&per6clk,
	&hdmiclk,
	&apeatclk,
	&apetraceclk,
	&mcdeclk,
	&dsialtclk,
	&dmaclk,
	&b2r2clk,
	&tvclk,
	&rngclk,
	&pwmclk,

	/* PRCC clocks */
	&p1_pclk0,
	&p1_pclk1,
	&p1_pclk2,
	&p1_pclk3,
	&p1_pclk4,
	&p1_pclk5,
	&p1_pclk6,

	&p2_pclk0,
	&p2_pclk1,

	&p3_pclk0,
	&p3_pclk1,
	&p3_pclk2,

	&p5_pclk0,
	&p5_pclk1,
	&p5_pclk2,
	&p5_pclk3,
	&p5_pclk4,
	&p5_pclk5,
	&p5_pclk6,
	&p5_pclk7,
	&p5_pclk8,
	&p5_pclk9,
	&p5_pclk10,
	&p5_pclk11,
	&p5_pclk12,
	&p5_pclk13,
	&p5_pclk14,
	&p5_pclk15,

	&p6_pclk0,
	&p6_pclk1,
	&p6_pclk2,
	&p6_pclk3,
	&p6_pclk4,
	&p6_pclk5,
	&p6_pclk6,
	&p6_pclk7,

	/* Clock sources */
	&sysclk2,
	&clkout0,
	&clkout1,
	&rtc_clk1,
};

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

static struct clk_lookup db5500_prcmu_clocks[] = {
	CLK_LOOKUP(sgaclk, "mali", NULL),
	CLK_LOOKUP(siaclk, "mmio_camera", "sia"),
	CLK_LOOKUP(svaclk, "hva", NULL),
	CLK_LOOKUP(uartclk, "UART", NULL),
	CLK_LOOKUP(msp02clk, "MSP02", NULL),
	CLK_LOOKUP(i2cclk, "I2C", NULL),
	CLK_LOOKUP(sdmmcclk, "sdmmc", NULL),
	CLK_LOOKUP(per1clk, "PERIPH1", NULL),
	CLK_LOOKUP(per2clk, "PERIPH2", NULL),
	CLK_LOOKUP(per3clk, "PERIPH3", NULL),
	CLK_LOOKUP(per5clk, "PERIPH5", NULL),
	CLK_LOOKUP(per6clk, "PERIPH6", NULL),
	CLK_LOOKUP(hdmiclk, "mcde", "hdmi"),
	CLK_LOOKUP(apeatclk, "apeat", NULL),
	CLK_LOOKUP(apetraceclk, "apetrace", NULL),
	CLK_LOOKUP(mcdeclk, "mcde", NULL),
	CLK_LOOKUP(mcdeclk, "mcde", "mcde"),
	CLK_LOOKUP(dmaclk, "dma40.0", NULL),
	CLK_LOOKUP(b2r2clk, "b2r2", NULL),
	CLK_LOOKUP(b2r2clk, "b2r2_bus", NULL),
	CLK_LOOKUP(b2r2clk, "U8500-B2R2.0", NULL),
	CLK_LOOKUP(tvclk, "tv", NULL),
	CLK_LOOKUP(tvclk, "mcde", "tv"),
};

static struct clk_lookup db5500_prcc_clocks[] = {
	CLK_LOOKUP(p1_msp0_clk, "MSP_I2S.0", NULL),
	CLK_LOOKUP(p1_sdi0_clk, "sdi0", NULL),
	CLK_LOOKUP(p1_sdi2_clk, "sdi2", NULL),
	CLK_LOOKUP(p1_uart0_clk, "uart0", NULL),
	CLK_LOOKUP(p1_i2c1_clk, "nmk-i2c.1", NULL),
	CLK_LOOKUP(p1_pclk5, "gpio.0", NULL),
	CLK_LOOKUP(p1_pclk5, "gpio.1", NULL),
	CLK_LOOKUP(p1_pclk6, "fsmc", NULL),

	CLK_LOOKUP(p2_pclk0, "musb_hdrc.0", NULL),
	CLK_LOOKUP(p2_pclk1, "gpio.2", NULL),

	CLK_LOOKUP(p3_keypad_clk, "db5500-keypad", NULL),
	CLK_LOOKUP(p3_pwm_clk, "pwm", NULL),
	CLK_LOOKUP(p3_pclk2, "gpio.4", NULL),

	CLK_LOOKUP(p5_msp2_clk, "MSP_I2S.2", NULL),
	CLK_LOOKUP(p5_uart1_clk, "uart1", NULL),
	CLK_LOOKUP(p5_uart2_clk, "uart2", NULL),
	CLK_LOOKUP(p5_uart3_clk, "uart3", NULL),
	CLK_LOOKUP(p5_sdi1_clk, "sdi1", NULL),
	CLK_LOOKUP(p5_sdi3_clk, "sdi3", NULL),
	CLK_LOOKUP(p5_sdi4_clk, "sdi4", NULL),
	CLK_LOOKUP(p5_i2c2_clk, "nmk-i2c.2", NULL),
	CLK_LOOKUP(p5_i2c3_clk, "nmk-i2c.3", NULL),
	CLK_LOOKUP(p5_irrc_clk, "irrc", NULL),
	CLK_LOOKUP(p5_irda_clk, "irda", NULL),
	CLK_LOOKUP(p5_pclk11, "spi0", NULL),
	CLK_LOOKUP(p5_pclk12, "spi1", NULL),
	CLK_LOOKUP(p5_pclk13, "spi2", NULL),
	CLK_LOOKUP(p5_pclk14, "spi3", NULL),
	CLK_LOOKUP(p5_pclk15, "gpio.5", NULL),
	CLK_LOOKUP(p5_pclk15, "gpio.6", NULL),
	CLK_LOOKUP(p5_pclk15, "gpio.7", NULL),

	CLK_LOOKUP(p6_rng_clk, "rng", NULL),
	CLK_LOOKUP(p6_pclk1, "cryp", NULL),
	CLK_LOOKUP(p6_pclk2, "hash0", NULL),
	CLK_LOOKUP(p6_pclk3, "pka", NULL),
	CLK_LOOKUP(p6_pclk4, "hash1", NULL),
	CLK_LOOKUP(p6_pclk5, "cfgreg", NULL),
	CLK_LOOKUP(p6_mtu0_clk, "mtu0", NULL),
	CLK_LOOKUP(p6_mtu1_clk, "mtu1", NULL),

	/*
	 * TODO: Clarify whether MSP1 need to be accessed from Linux, and who
	 * sets up the GPIOs.
	 */
	CLK_LOOKUP(clk_dummy, "gpio.3", NULL),
	CLK_LOOKUP(clk_msp1, "MSP_I2S.1", NULL),
};

static struct clk_lookup db5500_clkouts[] = {
	CLK_LOOKUP(clkout1, "mmio_camera", "primary-cam"),
	CLK_LOOKUP(clkout1, "mmio_camera", "secondary-cam"),
};

static struct clk_lookup u5500_clocks[] = {
	CLK_LOOKUP(rtc_clk1, "cg2900-uart.0", "lpoclk"),
};

static const char *db5500_boot_clk[] __initdata = {
	"spi0",
	"spi1",
	"spi2",
	"spi3",
	"uart0",
	"uart1",
	"uart2",
	"uart3",
	"sdi0",
	"sdi1",
	"sdi2",
	"sdi3",
	"sdi4",
};

static struct clk *boot_clks[ARRAY_SIZE(db5500_boot_clk)] __initdata;

static int __init db5500_boot_clk_disable(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(db5500_boot_clk); i++) {
		clk_disable(boot_clks[i]);
		clk_put(boot_clks[i]);
	}

	return 0;
}
late_initcall_sync(db5500_boot_clk_disable);

static void __init db5500_boot_clk_enable(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(db5500_boot_clk); i++) {
		boot_clks[i] = clk_get_sys(db5500_boot_clk[i], NULL);
		BUG_ON(IS_ERR(boot_clks[i]));
		clk_enable(boot_clks[i]);
	}
}

int __init db5500_clk_init(void)
{
	sysclk_ops.enable = NULL;
	sysclk_ops.disable = NULL;

	if (ux500_is_svp()) {
		prcmu_clk_ops.enable = NULL;
		prcmu_clk_ops.disable = NULL;
		prcc_pclk_ops.enable = NULL;
		prcc_pclk_ops.disable = NULL;
		prcc_kclk_ops.enable = NULL;
		prcc_kclk_ops.disable = NULL;
	}

	clks_register(u8500_common_clock_sources,
		ARRAY_SIZE(u8500_common_clock_sources));

	clks_register(db5500_prcmu_clocks, ARRAY_SIZE(db5500_prcmu_clocks));
	clks_register(db5500_prcc_clocks, ARRAY_SIZE(db5500_prcc_clocks));
	clks_register(db5500_clkouts, ARRAY_SIZE(db5500_clkouts));
	clks_register(u5500_clocks, ARRAY_SIZE(u5500_clocks));

	if (cpu_is_u8500v2()) {
		clks_register(u8500_v2_sysclks,
			ARRAY_SIZE(u8500_v2_sysclks));
	}

	db5500_boot_clk_enable();

	/*
	 * The following clks are shared with secure world.
	 * Currently this leads to a limitation where we need to
	 * enable them at all times.
	 */
	clk_enable(&p6_pclk1);
	clk_enable(&p6_pclk2);
	clk_enable(&p6_pclk3);
	clk_enable(&p6_rng_clk);

	return 0;
}

static int __init db5500_clk_debug_init(void)
{
	return dbx500_clk_debug_init(db5500_dbg_clks,
				     ARRAY_SIZE(db5500_dbg_clks));
}
module_init(db5500_clk_debug_init);
