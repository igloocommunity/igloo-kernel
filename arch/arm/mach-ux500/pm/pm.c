/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for
 *         ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#include <linux/io.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/hardware/gic.h>

#include <mach/hardware.h>
#include <mach/prcmu-regs.h>
#include <mach/gpio.h>

#define STABILIZATION_TIME 30 /* us */

#define PRCM_ARM_WFI_STANDBY_CPU0_WFI 0x8
#define PRCM_ARM_WFI_STANDBY_CPU1_WFI 0x10

static u32 u8500_gpio_banks[] = {U8500_GPIOBANK0_BASE,
				 U8500_GPIOBANK1_BASE,
				 U8500_GPIOBANK2_BASE,
				 U8500_GPIOBANK3_BASE,
				 U8500_GPIOBANK4_BASE,
				 U8500_GPIOBANK5_BASE,
				 U8500_GPIOBANK6_BASE,
				 U8500_GPIOBANK7_BASE,
				 U8500_GPIOBANK8_BASE};

static u32 u5500_gpio_banks[] = {U5500_GPIOBANK0_BASE,
				 U5500_GPIOBANK1_BASE,
				 U5500_GPIOBANK2_BASE,
				 U5500_GPIOBANK3_BASE,
				 U5500_GPIOBANK4_BASE,
				 U5500_GPIOBANK5_BASE,
				 U5500_GPIOBANK6_BASE,
				 U5500_GPIOBANK7_BASE};

static u32 ux500_gpio_wks[ARRAY_SIZE(u8500_gpio_banks)];
#ifdef ENABLE_ARM_FREQ_RAMP
/*
 * Ramp down the ARM frequency in order to reduce voltage
 * overshoot/undershoot
 */
int ux500_pm_arm_on_ext_clk(bool leave_arm_pll_on)
{
	u32 val;
	int divps_rate;

	/*
	 * TODO: we should check that there is no ongoing
	 * OPP change because then our writings could collide with the PRCMU.
	 */

	val = readl(PRCM_ARM_CHGCLKREQ);

	if (val & PRCM_ARM_CHGCLKREQ_PRCM_ARM_CHGCLKREQ)
		return -EINVAL;

	val = readl(PRCM_ARM_PLLDIVPS);

	/*
	 * TODO: Investigate if ramp down should start
	 * from current frequency.
	 */
	if (cpu_is_u8500v20_or_later()) {

		/*
		 * Store the current rate value. Is needed if
		 * we need to restore the frequency
		 */
		divps_rate = val & PRCM_ARM_PLLDIVPS_ARM_BRM_RATE;
		val &= ~PRCM_ARM_PLLDIVPS_ARM_BRM_RATE;

		/* Slow down the cpu's */
		if (divps_rate > 11) {
			writel(val | 11,  PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate > 5) {
			writel(val | 5, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate > 2)
			writel(val | 2, PRCM_ARM_PLLDIVPS);
	} else {

		divps_rate = val & PRCM_ARM_PLLDIVPS_MAX_MASK;
		val &= ~PRCM_ARM_PLLDIVPS_MAX_MASK;

		/* Slow down the cpu's */
		if (divps_rate < 3) {
			writel(val | 3, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate < 7) {
			writel(val | 7, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate < 15)
			writel(val | 15, PRCM_ARM_PLLDIVPS);
	}

	/* switch to external clock */
	writel(readl(PRCM_ARM_CHGCLKREQ) |
	       PRCM_ARM_CHGCLKREQ_PRCM_ARM_CHGCLKREQ,
	       PRCM_ARM_CHGCLKREQ);

	val = readl(PRCM_PLLARM_ENABLE);

	if (leave_arm_pll_on)
		/* Leave ARM PLL on */
		writel(val & (~PRCM_PLLARM_ENABLE_PRCM_PLLARM_COUNTON),
		       PRCM_PLLARM_ENABLE);
	else
		/* Stop ARM PLL */
		writel(val & (~PRCM_PLLARM_ENABLE_PRCM_PLLARM_ENABLE),
		       PRCM_PLLARM_ENABLE);
	return divps_rate;
}
#else
inline int ux500_pm_arm_on_ext_clk(bool leave_arm_pll_on)
{
	return 0;
}
#endif

#ifdef ENABLE_ARM_FREQ_RAMP
void ux500_pm_arm_on_arm_pll(int divps_rate)
{
	u32 pll_arm;
	u32 clk_req;
	u32 val;

	if (divps_rate < 0)
		return;

	clk_req = readl(PRCM_ARM_CHGCLKREQ);

	/* Return, if not running on external pll */
	if (!(clk_req & PRCM_ARM_CHGCLKREQ_PRCM_ARM_CHGCLKREQ))
		return;

	pll_arm = readl(PRCM_PLLARM_ENABLE);

	if (pll_arm & PRCM_PLLARM_ENABLE_PRCM_PLLARM_ENABLE) {
		/* ARM PLL is still on, set "counton" bit */
		writel(pll_arm | PRCM_PLLARM_ENABLE_PRCM_PLLARM_COUNTON,
		       PRCM_PLLARM_ENABLE);
	} else {
		/* ARM PLL was stopped => turn on */
		writel(pll_arm | PRCM_PLLARM_ENABLE_PRCM_PLLARM_ENABLE,
		       PRCM_PLLARM_ENABLE);

		/* Wait for PLL to lock */
		while (!(readl(PRCM_PLLARM_LOCKP) &
			 PRCM_PLLARM_LOCKP_PRCM_PLLARM_LOCKP3))
			cpu_relax();
	}

	writel(clk_req & ~PRCM_ARM_CHGCLKREQ_PRCM_ARM_CHGCLKREQ,
	       PRCM_ARM_CHGCLKREQ);

	val = readl(PRCM_ARM_PLLDIVPS);

	if (cpu_is_u8500v20_or_later()) {
		val &= ~PRCM_ARM_PLLDIVPS_ARM_BRM_RATE;

		/* Ramp up the ARM PLL */
		if (divps_rate >= 2) {
			writel(val | 2, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate >= 5) {
			writel(val | 5, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate >= 11)
			writel(val | 11, PRCM_ARM_PLLDIVPS);
	} else {
		val &= ~PRCM_ARM_PLLDIVPS_MAX_MASK;
		/* Ramp up the ARM PLL */
		if (divps_rate <= 15) {
			writel(val | 15, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate <= 7) {
			writel(val | 7, PRCM_ARM_PLLDIVPS);
			/* Wait for voltage to stabilize */
			udelay(STABILIZATION_TIME);
		}
		if (divps_rate <= 3)
			writel(val | 3, PRCM_ARM_PLLDIVPS);
	}
}
#else
inline void ux500_pm_arm_on_arm_pll(int divps_rate)
{
}
#endif

/* Decouple GIC from the interrupt bus */
void ux500_pm_gic_decouple(void)
{
	writel(readl(PRCM_A9_MASK_REQ) | PRCM_A9_MASK_REQ_PRCM_A9_MASK_REQ,
		PRCM_A9_MASK_REQ);

	while (!readl(PRCM_A9_MASK_REQ))
		cpu_relax();

	/* TODO: Use the ack bit when possible */
}

/* Recouple GIC with the interrupt bus */
void ux500_pm_gic_recouple(void)
{
	writel((readl(PRCM_A9_MASK_REQ) & ~PRCM_A9_MASK_REQ_PRCM_A9_MASK_REQ),
	       PRCM_A9_MASK_REQ);

	/* TODO: Use the ack bit when possible */
}

#define GIC_NUMBER_REGS 5
bool ux500_pm_gic_pending_interrupt(void)
{
	u32 pr; /* Pending register */
	u32 er; /* Enable register */
	int i;

	/* 5 registers. STI & PPI not skipped */
	for (i = 0; i < GIC_NUMBER_REGS; i++) {

		pr = readl(__io_address(U8500_GIC_DIST_BASE) +
			   GIC_DIST_PENDING_SET + i * 4);
		er = readl(__io_address(U8500_GIC_DIST_BASE) +
			   GIC_DIST_ENABLE_SET + i * 4);

		if (pr & er)
			return true; /* There is a pending interrupt */

	}

	return false;
}

#define GIC_NUMBER_SPI_REGS 4
bool ux500_pm_prcmu_pending_interrupt(void)
{
	u32 it;
	u32 im;
	int i;

	for (i = 0; i < GIC_NUMBER_SPI_REGS; i++) { /* There are 4 registers */

		it = readl(PRCM_ARMITVAL31TO0 + i * 4);
		im = readl(PRCM_ARMITMSK31TO0 + i * 4);

		if (it & im)
			return true; /* There is a pending interrupt */
	}

	return false;
}

void ux500_pm_prcmu_set_ioforce(bool enable)
{
	if (enable)
		writel(readl(PRCM_IOCR) | PRCM_IOCR_IOFORCE, PRCM_IOCR);
	else
		writel(readl(PRCM_IOCR) & ~PRCM_IOCR_IOFORCE, PRCM_IOCR);
}

void ux500_pm_prcmu_copy_gic_settings(void)
{
	u32 er; /* Enable register */
	int i;

	for (i = 0; i < GIC_NUMBER_SPI_REGS; i++) { /* 4*32 SPI interrupts */
		/* +1 due to skip STI and PPI */
		er = readl(IO_ADDRESS(U8500_GIC_DIST_BASE) +
			   GIC_DIST_ENABLE_SET + (i + 1) * 4);
		writel(er, PRCM_ARMITMSK31TO0 + i * 4);
	}
}

void ux500_pm_gpio_save_wake_up_status(void)
{
	int num_banks;
	u32 *banks;
	int i;

	if (cpu_is_u5500()) {
		num_banks = ARRAY_SIZE(u5500_gpio_banks);
		banks = u5500_gpio_banks;
	} else {
		num_banks = ARRAY_SIZE(u8500_gpio_banks);
		banks = u8500_gpio_banks;
	}

	for (i = 0; i < num_banks; i++)
		ux500_gpio_wks[i] = readl(IO_ADDRESS(banks[i]) + NMK_GPIO_WKS);
}

u32 ux500_pm_gpio_read_wake_up_status(unsigned int bank_num)
{
	if (WARN_ON(cpu_is_u5500() && bank_num >=
		    ARRAY_SIZE(u5500_gpio_banks)))
		return 0;

	if (WARN_ON(cpu_is_u8500() && bank_num >=
		    ARRAY_SIZE(u8500_gpio_banks)))
		return 0;

	return ux500_gpio_wks[bank_num];
}

/* Check if the other CPU is in WFI */
bool ux500_pm_other_cpu_wfi(void)
{
	if (smp_processor_id()) {
		/* We are CPU 1 => check if CPU0 is in WFI */
		if (readl(PRCM_ARM_WFI_STANDBY) &
		    PRCM_ARM_WFI_STANDBY_CPU0_WFI)
			return true;
	} else {
		/* We are CPU 0 => check if CPU1 is in WFI */
		if (readl(PRCM_ARM_WFI_STANDBY) &
		    PRCM_ARM_WFI_STANDBY_CPU1_WFI)
			return true;
	}

	return false;
}
