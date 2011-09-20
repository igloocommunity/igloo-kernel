/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 *
 * Authors: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	    Jonas Aaberg <jonas.aberg@stericsson.com>,
 *          Sundar Iyer for ST-Ericsson.
 */

#include <linux/suspend.h>
#include <linux/mfd/dbx500-prcmu.h>

#include <mach/context.h>
#include <mach/pm.h>

static int suspend(bool do_deepsleep)
{
	int ret = 0;

	/* configure the prcm for a sleep wakeup */
	prcmu_enable_wakeups(PRCMU_WAKEUP(ABB));

	context_vape_save();

	ux500_pm_gic_decouple();

	if (ux500_pm_gic_pending_interrupt()) {
		pr_info("suspend/resume: pending interrupt\n");

		/* Recouple GIC with the interrupt bus */
		ux500_pm_gic_recouple();
		ret = -EBUSY;

		goto exit;
	}
	ux500_pm_prcmu_set_ioforce(true);

	if (do_deepsleep) {
		context_varm_save_common();
		context_varm_save_core();
		context_gic_dist_disable_unneeded_irqs();
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

	/* APE was turned off, restore IO ring */
	ux500_pm_prcmu_set_ioforce(false);

exit:
	/* This is what cpuidle wants */
	prcmu_enable_wakeups(PRCMU_WAKEUP(ARM) | PRCMU_WAKEUP(RTC) |
			     PRCMU_WAKEUP(ABB));
	return ret;
}

static int ux500_suspend_enter(suspend_state_t state)
{
	if (state == PM_SUSPEND_MEM)
		return suspend(true);
	else if (state == PM_SUSPEND_STANDBY)
		return suspend(false);
	else
		return -EINVAL;
}

static int ux500_suspend_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM || state == PM_SUSPEND_STANDBY;
}

static int ux500_suspend_prepare_late(void)
{
	/* ESRAM to retention instead of OFF until ROM is fixed */
	(void) prcmu_config_esram0_deep_sleep(ESRAM0_DEEP_SLEEP_STATE_RET);
	return 0;
}

static void ux500_suspend_wake(void)
{
	(void) prcmu_config_esram0_deep_sleep(ESRAM0_DEEP_SLEEP_STATE_RET);
}

static int ux500_suspend_begin(suspend_state_t state)
{
	(void) prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
					    "suspend", 100);
	return 0;
}

static void ux500_suspend_end(void)
{
	(void) prcmu_qos_update_requirement(PRCMU_QOS_ARM_OPP,
					    "suspend", 25);
}

static struct platform_suspend_ops ux500_suspend_ops = {
	.enter	      = ux500_suspend_enter,
	.valid	      = ux500_suspend_valid,
	.prepare_late = ux500_suspend_prepare_late,
	.wake	      = ux500_suspend_wake,
	.begin	      = ux500_suspend_begin,
	.end	      = ux500_suspend_end,
};

static __init int ux500_suspend_init(void)
{
	prcmu_qos_add_requirement(PRCMU_QOS_ARM_OPP, "suspend", 25);
	suspend_set_ops(&ux500_suspend_ops);
	return 0;
}
device_initcall(ux500_suspend_init);
