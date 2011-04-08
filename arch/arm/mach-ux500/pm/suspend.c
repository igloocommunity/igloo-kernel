/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Authors: Rickard Andersson <rickard.andersson@stericsson.com>,
 *          Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 *
 */

#include <linux/suspend.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/ab8500-debug.h>

#include <mach/prcmu-fw-api.h>
#include <mach/prcmu-regs.h>

#include "context.h"
#include "pm.h"
#include "suspend_dbg.h"

extern void mop500_pins_suspend_force(void);
extern void mop500_pins_suspend_force_mux(void);

static atomic_t block_sleep = ATOMIC_INIT(0);

void suspend_block_sleep(void)
{
	atomic_inc(&block_sleep);
}

void suspend_unblock_sleep(void)
{
	atomic_dec(&block_sleep);
}

static bool sleep_is_blocked(void)
{
	return (atomic_read(&block_sleep) != 0);
}

static int suspend(bool do_deepsleep)
{
	u32 divps_rate;

	if (sleep_is_blocked()) {
		pr_info("suspend/resume: interrupted by modem.\n");
		return -EBUSY;
	}

	ux500_suspend_dbg_add_wake_on_uart();
	nmk_gpio_wakeups_suspend();

	/* configure the prcm for a sleep wakeup */
	prcmu_enable_wakeups(PRCMU_WAKEUP(ABB));

	context_vape_save();

	/*
	 * Save GPIO settings before applying power save
	 * settings
	 */
	context_gpio_save();

	/* Apply GPIO power save mux settings */
	context_gpio_mux_safe_switch(true);
	mop500_pins_suspend_force_mux();
	context_gpio_mux_safe_switch(false);

	/* Apply GPIO power save settings */
	mop500_pins_suspend_force();

	ux500_pm_gic_decouple();

	/* TODO: decouple gic should look at status bit.*/
	udelay(100);

	divps_rate = ux500_pm_arm_on_ext_clk(false);

	if (ux500_pm_gic_pending_interrupt()) {
		prcmu_disable_wakeups();
		nmk_gpio_wakeups_resume();
		ux500_suspend_dbg_remove_wake_on_uart();

		ux500_pm_arm_on_arm_pll(divps_rate);
		/* Recouple GIC with the interrupt bus */
		ux500_pm_gic_recouple();
		pr_info("suspend/resume: pending interrupt\n");
		return -EBUSY;
	}
	ux500_pm_prcmu_set_ioforce(true);

	if (do_deepsleep) {
		context_varm_save_common();
		context_varm_save_core();
		context_save_cpu_registers();

		/*
		 * Due to we have only 100us between requesting a powerstate
		 * and wfi, we clean the cache before as well to assure the
		 * final cache clean before wfi has as little as possible to
		 * do.
		 */
		context_clean_l1_cache_all();

		(void) prcmu_set_power_state(PRCMU_AP_DEEP_SLEEP,
					     false, false);
		context_save_to_sram_and_wfi(true);

		context_restore_cpu_registers();
		context_varm_restore_core();
		context_varm_restore_common();

	} else {

		context_clean_l1_cache_all();
		(void) prcmu_set_power_state(APEXECUTE_TO_APSLEEP,
					     false, false);
		dsb();
		__asm__ __volatile__("wfi\n\t" : : : "memory");
	}

	context_vape_restore();

	/* If GPIO woke us up then save the pins that caused the wake up */
	ux500_pm_gpio_save_wake_up_status();

	ux500_suspend_dbg_sleep_status(do_deepsleep);

	/* APE was turned off, restore IO ring */
	ux500_pm_prcmu_set_ioforce(false);

	/* Restore gpio settings */
	context_gpio_mux_safe_switch(true);
	context_gpio_restore_mux();
	context_gpio_mux_safe_switch(false);
	context_gpio_restore();

	prcmu_disable_wakeups();

	nmk_gpio_wakeups_resume();
	ux500_suspend_dbg_remove_wake_on_uart();

	return 0;
}

static int ux500_suspend_enter(suspend_state_t state)
{

	if (ux500_suspend_enabled()) {
		if (ux500_suspend_deepsleep_enabled() &&
		    state == PM_SUSPEND_MEM)
			return suspend(true);
		if (ux500_suspend_sleep_enabled())
			return suspend(false);
		/* For debugging, if Sleep and DeepSleep disabled, do Idle */
		prcmu_set_power_state(PRCMU_AP_IDLE, true, true);
	}

	dsb();
	__asm__ __volatile__("wfi\n\t" : : : "memory");
	return 0;
}

static int ux500_suspend_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM || state == PM_SUSPEND_STANDBY;
}

static int ux500_suspend_prepare_late(void)
{
	(void)prcmu_config_esram0_deep_sleep(ESRAM0_DEEP_SLEEP_STATE_OFF);
	ab8500_regulator_debug_force();

	return 0;
}

static void ux500_suspend_wake(void)
{
	ab8500_regulator_debug_restore();
	(void)prcmu_config_esram0_deep_sleep(ESRAM0_DEEP_SLEEP_STATE_RET);
}

static struct platform_suspend_ops ux500_suspend_ops = {
	.enter        = ux500_suspend_enter,
	.valid        = ux500_suspend_valid,
	.prepare_late = ux500_suspend_prepare_late,
	.wake         = ux500_suspend_wake,
	.begin        = ux500_suspend_dbg_begin,
};

static __init int ux500_suspend_init(void)
{
	ux500_suspend_dbg_init();
	suspend_set_ops(&ux500_suspend_ops);
	return 0;
}

device_initcall(ux500_suspend_init);
