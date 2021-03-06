/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for
 *         ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#ifndef PM_COMMON_H
#define PM_COMMON_H

#ifdef CONFIG_PM
enum prcmu_idle_stat {
	SLEEP_OK		= 0xf3,
	DEEP_SLEEP_OK		= 0xf6,
	IDLE_OK			= 0xf0,
	DEEPIDLE_OK		= 0xe3,
	PRCMU2ARMPENDINGIT_ER	= 0x91,
	ARMPENDINGIT_ER		= 0x93,
};

/**
 * ux500_pm_gic_decouple()
 *
 * Decouple GIC from the interrupt bus.
 */
void ux500_pm_gic_decouple(void);

/**
 * ux500_pm_gic_recouple()
 *
 * Recouple GIC with the interrupt bus.
 */
void ux500_pm_gic_recouple(void);

/**
 * ux500_pm_gic_pending_interrupt()
 *
 * returns true, if there are pending interrupts.
 */
bool ux500_pm_gic_pending_interrupt(void);

/**
 * ux500_pm_prcmu_pending_interrupt()
 *
 * returns true, if there are pending interrupts.
 */
bool ux500_pm_prcmu_pending_interrupt(void);

/**
 * ux500_pm_prcmu_set_ioforce()
 *
 * @enable: Enable/disable
 *
 * Enable/disable the gpio-ring
 */
void ux500_pm_prcmu_set_ioforce(bool enable);

/**
 * ux500_pm_prcmu_copy_gic_settings()
 *
 * This function copies all the gic interrupt settings to the prcmu.
 * This is needed for the system to catch interrupts in ApIdle
 */
void ux500_pm_prcmu_copy_gic_settings(void);

/**
 * ux500_pm_gpio_save_wake_up_status()
 *
 * This function is called when the prcmu has woken the ARM
 * but before ioforce is disabled.
 */
void ux500_pm_gpio_save_wake_up_status(void);

/**
 * ux500_pm_gpio_read_wake_up_status()
 *
 * @bank_number: The gpio bank.
 *
 * Returns the WKS register settings for given bank number.
 * The WKS register is cleared when ioforce is released therefore
 * this function is needed.
 */
u32 ux500_pm_gpio_read_wake_up_status(unsigned int bank_number);

/**
 * ux500_pm_other_cpu_wfi()
 *
 * Returns true if the other CPU is in WFI.
 */
bool ux500_pm_other_cpu_wfi(void);

/**
 * ux500_pm_prcmu_idle_stat()
 *
 * Returns the status of the last prcmu idle/sleep
 */
enum prcmu_idle_stat ux500_pm_prcmu_idle_stat(void);

struct dev_pm_domain;
extern struct dev_pm_domain ux500_dev_power_domain;
extern struct dev_pm_domain ux500_amba_dev_power_domain;

#else
u32 ux500_pm_gpio_read_wake_up_status(unsigned int bank_number)
{
	return 0;
}

/**
 * ux500_pm_prcmu_set_ioforce()
 *
 * @enable: Enable/disable
 *
 * Enable/disable the gpio-ring
 */
static inline void ux500_pm_prcmu_set_ioforce(bool enable) { }

#endif

#endif
