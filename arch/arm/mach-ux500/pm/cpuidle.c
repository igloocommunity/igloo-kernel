/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for
 *         ST-Ericsson. Loosly based on cpuidle.c by Sundar Iyer.
 * License terms: GNU General Public License (GPL) version 2
 *
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/tick.h>

#include <plat/gpio.h>

#include <mach/prcmu-fw-api.h>

#include "cpuidle.h"
#include "cpuidle_dbg.h"
#include "context.h"
#include "pm.h"
#include "../regulator-db8500.h"
#include "../timer-rtt.h"

#define DEEP_SLEEP_WAKE_UP_LATENCY 8500
#define SLEEP_WAKE_UP_LATENCY 800
#define UL_PLL_START_UP_LATENCY 8000 /* us */
#define RTC_PROGRAM_TIME 400 /* us */

static struct cstate cstates[] = {
	{
		.enter_latency = 0,
		.exit_latency = 0,
		.threshold = 0,
		.power_usage = 1000,
		.APE = APE_ON,
		.ARM = ARM_ON,
		.ARM_PLL = ARM_PLL_ON,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = NO_TRANSITION,
		.state = CI_RUNNING,
		.desc = "Running                ",
	},
	{
		.enter_latency = 0,
		.exit_latency = 0,
		.threshold = 0,
		.power_usage = 10,
		.APE = APE_ON,
		.ARM = ARM_ON,
		.ARM_PLL = ARM_PLL_ON,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = NO_TRANSITION,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_WFI,
		.desc = "Wait for interrupt     ",
	},
	{
		.enter_latency = RTC_PROGRAM_TIME,
		.exit_latency = 450,
		.threshold = 500 + RTC_PROGRAM_TIME,
		.power_usage = 5,
		.APE = APE_ON,
		.ARM = ARM_RET,
		.ARM_PLL = ARM_PLL_ON,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_IDLE,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_IDLE,
		.desc = "ApIdle                 ",
	},
	{
		.enter_latency = RTC_PROGRAM_TIME,
		.exit_latency = 570,
		.threshold = 600 + RTC_PROGRAM_TIME,
		.power_usage = 4,
		.APE = APE_ON,
		.ARM = ARM_RET,
		.ARM_PLL = ARM_PLL_OFF,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_IDLE,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_IDLE,
		.desc = "ApIdle, ARM PLL off    ",
	},
	{
		.enter_latency = RTC_PROGRAM_TIME + 50,
		.exit_latency = SLEEP_WAKE_UP_LATENCY,
		.threshold = 800 + RTC_PROGRAM_TIME,
		.power_usage = 3,
		.APE = APE_OFF,
		.ARM = ARM_RET,
		.ARM_PLL = ARM_PLL_OFF,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_SLEEP,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_SLEEP,
		.desc = "ApSleep                ",
	},
	{
		.enter_latency = RTC_PROGRAM_TIME + 50,
		.exit_latency = (SLEEP_WAKE_UP_LATENCY +
				   UL_PLL_START_UP_LATENCY),
		.threshold = (2 * (SLEEP_WAKE_UP_LATENCY +
				   UL_PLL_START_UP_LATENCY + 50) +
			      RTC_PROGRAM_TIME),
		.power_usage = 2,
		.APE = APE_OFF,
		.ARM = ARM_RET,
		.ARM_PLL = ARM_PLL_OFF,
		.UL_PLL = UL_PLL_OFF,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_SLEEP,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_SLEEP,
		.desc = "ApSleep, UL PLL off    ",
	},
#ifdef ENABLE_AP_DEEP_IDLE
	{
		.enter_latency = RTC_PROGRAM_TIME + 200,
		.exit_latency = (DEEP_SLEEP_WAKE_UP_LATENCY +
				   RTC_PROGRAM_TIME),
		.threshold = 8700,
		.power_usage = 2,
		.APE = APE_ON,
		.ARM = ARM_OFF,
		.ARM_PLL = ARM_PLL_OFF,
		.UL_PLL = UL_PLL_ON,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_DEEP_IDLE,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_DEEP_IDLE,
		.desc = "ApDeepIdle, UL PLL off ",
	},
#endif
	{
		.enter_latency = RTC_PROGRAM_TIME + 250,
		.exit_latency = (DEEP_SLEEP_WAKE_UP_LATENCY +
				   RTC_PROGRAM_TIME),
		.threshold = 9000,
		.power_usage = 1,
		.APE = APE_OFF,
		.ARM = ARM_OFF,
		.ARM_PLL = ARM_PLL_OFF,
		.UL_PLL = UL_PLL_OFF,
		.ESRAM = ESRAM_RET,
		.pwrst = PRCMU_AP_DEEP_SLEEP,
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.state = CI_DEEP_SLEEP,
		.desc = "ApDeepsleep, UL PLL off",
	},
};

struct cpu_state {
	bool restore_arm_core;
	bool ready_deep_sleep;
	bool always_on_timer_migrated;
	ktime_t sched_wake_up;
	struct cpuidle_device dev;
	int this_cpu;
};

static DEFINE_PER_CPU(struct cpu_state, *cpu_state);

static DEFINE_SPINLOCK(cpuidle_lock);
static bool restore_ape; /* protected by cpuidle_lock */
static bool restore_arm_common; /* protected by cpuidle_lock */

static atomic_t idle_cpus_counter = ATOMIC_INIT(0);

struct cstate *ux500_ci_get_cstates(int *len)
{
	if (len != NULL)
		(*len) = ARRAY_SIZE(cstates);
	return cstates;
}

/*
 * cpuidle & hotplug - plug or unplug a cpu in idle sequence
 */
void ux500_cpuidle_plug(int cpu)
{
	atomic_dec(&idle_cpus_counter);
	wmb();
}
void ux500_cpuidle_unplug(int cpu)
{
	atomic_inc(&idle_cpus_counter);
	wmb();
}

static void migrate_to_always_on_timer(struct cpu_state *state)
{
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER,
			   &state->this_cpu);

	state->always_on_timer_migrated = true;
	smp_wmb();
}

static void migrate_to_local_timer(struct cpu_state *state)
{
	/* Use the ARM local timer for this cpu */
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT,
			   &state->this_cpu);

	state->always_on_timer_migrated = false;
	smp_wmb();
}

static void restore_sequence(struct cpu_state *state, bool slept_well)
{
	unsigned long iflags;
	ktime_t t;

	if (!slept_well)
		/* Recouple GIC with the interrupt bus */
		ux500_pm_gic_recouple();

	spin_lock_irqsave(&cpuidle_lock, iflags);

	if (slept_well) {
		/*
		 * Remove wake up time i.e. set wake up
		 * far ahead
		 */
		t = ktime_add_us(ktime_get(),
				 1000000000); /* 16 minutes ahead */
		state->sched_wake_up = t;
		smp_wmb();
	}

	smp_rmb();
	if (state->restore_arm_core) {

		state->restore_arm_core = false;
		smp_wmb();

		context_varm_restore_core();
	}

	smp_rmb();
	if (restore_arm_common) {

		restore_arm_common = false;
		smp_wmb();

		/* Restore gic settings */
		context_varm_restore_common();
	}

	smp_rmb();
	if (restore_ape) {

		restore_ape = false;
		smp_wmb();

		/*
		 * APE has been turned off. Save GPIO wake up cause before
		 * clearing ioforce.
		 */
		context_vape_restore();

		ux500_pm_gpio_save_wake_up_status();

		/* Restore IO ring */
		ux500_pm_prcmu_set_ioforce(false);

		ux500_ci_dbg_console_handle_ape_resume();

	}

	spin_unlock_irqrestore(&cpuidle_lock, iflags);

	smp_rmb();
	if (state->always_on_timer_migrated)
		migrate_to_local_timer(state);


}

/**
 * get_remaining_sleep_time() - returns remaining sleep time in
 * microseconds (us)
 */
static int get_remaining_sleep_time(void)
{
	ktime_t now;
	int cpu;
	unsigned long iflags;
	int t;
	int remaining_sleep_time = INT_MAX;

	now = ktime_get();

	/*
	 * Check next schedule to expire considering both
	 * cpus
	 */
	spin_lock_irqsave(&cpuidle_lock, iflags);
	for_each_online_cpu(cpu) {
		t = ktime_to_us(ktime_sub(per_cpu(cpu_state,
						  cpu)->sched_wake_up,
					  now));
		if (t < remaining_sleep_time)
			remaining_sleep_time = t;
	}
	spin_unlock_irqrestore(&cpuidle_lock, iflags);

	return remaining_sleep_time;
}

static bool cores_ready_deep_sleep(void)
{
	int cpu;
	int this_cpu;

	this_cpu = smp_processor_id();

	for_each_online_cpu(cpu) {
		if (cpu != this_cpu) {
			smp_rmb();
			if (!per_cpu(cpu_state, cpu)->ready_deep_sleep)
				return false;
		}
	}

	return true;

}

static bool cores_timer_migrated(void)
{
	int cpu;
	int this_cpu;

	this_cpu = smp_processor_id();

	for_each_online_cpu(cpu) {
		if (cpu != this_cpu) {
			smp_rmb();
			if (!per_cpu(cpu_state, cpu)->always_on_timer_migrated)
				return false;
		}
	}

	return true;

}

static int determine_sleep_state(int idle_cpus,
				 int gov_cstate)
{
	int i;
	int sleep_time;
	bool power_state_req;

	/* If first cpu to sleep, go to most shallow sleep state */
	if (idle_cpus < num_online_cpus())
		return CI_WFI;

	/* If other CPU is going to WFI, but not yet there wait. */
	while (1) {
		if (ux500_pm_other_cpu_wfi())
			break;

		if (ux500_pm_gic_pending_interrupt())
			return -1;

		if (atomic_read(&idle_cpus_counter) < num_online_cpus())
			return CI_WFI;
	}

	power_state_req = power_state_active_is_enabled() ||
		prcmu_is_ac_wake_requested();

	sleep_time = get_remaining_sleep_time();

	/*
	 * Never go deeper than the governor recommends even though it might be
	 * possible from a scheduled wake up point of view
	 */

	/*
	 * The variable "i" now contains the index of the deepest sleep state
	 * we can go to right now
	 */

	for (i = min(gov_cstate, ux500_ci_dbg_deepest_state()); i > 0; i--) {

		if (sleep_time <= cstates[i].threshold)
			continue;

		if ((cstates[i].ARM != ARM_ON) &&
		    !cores_timer_migrated())
			/*
			 * This sleep state needs timer migration,
			 * but other cpu has not migrated its timer
			 */
			continue;

		if (cstates[i].APE == APE_OFF) {
			/* This state says APE should be off */
			if (power_state_req ||
			    ux500_ci_dbg_force_ape_on())
				continue;
		}

		if ((cstates[i].ARM == ARM_OFF) &&
		    (!cores_ready_deep_sleep()))
			continue;

		/* OK state */
		break;
	}

	return max(CI_WFI, i);

}

static int enter_sleep(struct cpuidle_device *dev,
		       struct cpuidle_state *ci_state)
{
	ktime_t t1, t2, t3;
	s64 diff;
	int ret;
	bool pending_int;
	int cpu;
	int target;
	int gov_cstate;
	struct cpu_state *state;

	unsigned long iflags;
	int idle_cpus;
	u32 divps_rate;
	bool slept_well = false;

	local_irq_disable();

	t1 = ktime_get(); /* Time now */

	state = per_cpu(cpu_state, smp_processor_id());

	/* Save scheduled wake up for this cpu */
	spin_lock_irqsave(&cpuidle_lock, iflags);
	state->sched_wake_up = ktime_add(t1, tick_nohz_get_sleep_length());
	spin_unlock_irqrestore(&cpuidle_lock, iflags);

	/*
	 * Retrive the cstate that the governor recommends
	 * for this CPU
	 */
	gov_cstate = (int) cpuidle_get_statedata(ci_state);

	idle_cpus = atomic_inc_return(&idle_cpus_counter);

	/*
	 * Determine sleep state considering both CPUs and
	 * shared resources like e.g. VAPE
	 */
	target = determine_sleep_state(idle_cpus,
				       gov_cstate);
	if (target < 0) {
		/* "target" will be last_state in the cpuidle framework */
		target = CI_RUNNING;
		goto exit_fast;
	}

	if (cstates[target].ARM == ARM_ON) {


		switch(cstates[gov_cstate].ARM) {

		case ARM_OFF:
			ux500_ci_dbg_msg("WFI_prep");

			/*
			 * Can not turn off arm now, but it might be
			 * possible later, so prepare for it by saving
			 * context of cpu etc already now.
			 */

			/*
			 * ARM timers will stop during ARM retention or
			 * ARM off mode. Use always-on-timer instead.
			 */
			migrate_to_always_on_timer(state);

			context_varm_save_core();
			context_save_cpu_registers();
			state->ready_deep_sleep = true;
			smp_wmb();

			/*
			 * Save return address to SRAM and set this
			 * CPU in WFI
			 */
			ux500_ci_dbg_log(CI_WFI, t1);
			context_save_to_sram_and_wfi(false);

			t3 = ktime_get();

			state->ready_deep_sleep = false;
			smp_wmb();

			context_restore_cpu_registers();
			break;
		case ARM_RET:
			/*
			 * Can not go ApIdle or deeper now, but it
			 * might be possible later, so prepare for it
			 */

			/*
			 * ARM timers will stop during ARM retention or
			 * ARM off mode. Use always-on-timer instead.
			 */
			migrate_to_always_on_timer(state);
			/* Fall through */
		case ARM_ON:
			ux500_ci_dbg_msg("WFI");
			ux500_ci_dbg_log(CI_WFI, t1);
			__asm__ __volatile__
				("dsb\n\t" "wfi\n\t" : : : "memory");
			break;
		default:
			/* Cannot happen */
			break;
		}
		slept_well = true;
		goto exit;
	}

	/* Decouple GIC from the interrupt bus */
	ux500_pm_gic_decouple();

	if (!ux500_pm_other_cpu_wfi())
		/* Other CPU was not in WFI => abort */
		goto exit;

	prcmu_enable_wakeups(PRCMU_WAKEUP(ARM) | PRCMU_WAKEUP(RTC) |
			     PRCMU_WAKEUP(ABB));

	migrate_to_always_on_timer(state);

	/* Check pending interrupts */
	pending_int = ux500_pm_gic_pending_interrupt();

	/*
	 * Check if we have a pending interrupt or if sleep
	 * state has changed after GIC has been frozen
	 */
	if (pending_int ||
	    (target != determine_sleep_state(atomic_read(&idle_cpus_counter),
					     gov_cstate)))
		/* Pending interrupt or sleep state has changed => abort */
		goto exit;

	/*
	 * Copy GIC interrupt settings to
	 * PRCMU interrupt settings
	 */
	ux500_pm_prcmu_copy_gic_settings();

	/* Clean the cache before slowing down cpu frequency */
	context_clean_l1_cache_all();

	divps_rate = ux500_pm_arm_on_ext_clk(cstates[target].ARM_PLL);

	if (ux500_pm_prcmu_pending_interrupt()) {
		/* An interrupt found => abort */
		ux500_pm_arm_on_arm_pll(divps_rate);
		goto exit;
	}

	/*
	 * No PRCMU interrupt was pending => continue
	 * the sleeping stages
	 */

	/* Compensate for ULPLL start up time */
	if (cstates[target].UL_PLL == UL_PLL_OFF)
		(void) rtc_rtt_adjust_next_wakeup(-UL_PLL_START_UP_LATENCY);

	if (cstates[target].APE == APE_OFF) {

		/*
		 * We are going to sleep or deep sleep =>
		 * prepare for it
		 */

		context_vape_save();

		ux500_ci_dbg_console_handle_ape_suspend();
		ux500_pm_prcmu_set_ioforce(true);

		spin_lock_irqsave(&cpuidle_lock, iflags);
		restore_ape = true;
		smp_wmb();
		spin_unlock_irqrestore(&cpuidle_lock, iflags);
	}

	if (cstates[target].ARM == ARM_OFF) {
		/* Save gic settings */
		context_varm_save_common();

		context_varm_save_core();

		spin_lock_irqsave(&cpuidle_lock, iflags);

		restore_arm_common = true;
		for_each_online_cpu(cpu) {
			per_cpu(cpu_state, cpu)->restore_arm_core = true;
			smp_wmb();
		}

		spin_unlock_irqrestore(&cpuidle_lock, iflags);
	}


	context_save_cpu_registers();

	state->ready_deep_sleep = true;
	smp_wmb();

	/* TODO: To use desc as debug print might be a bad idea */
	ux500_ci_dbg_msg(cstates[target].desc);

	/*
	 * Due to we have only 100us between requesting a
	 * powerstate and wfi, we clean the cache before as
	 * well to assure the final cache clean before wfi
	 * has as little as possible to do.
	 */
	context_clean_l1_cache_all();

	ux500_ci_dbg_log(target, t1);

	prcmu_set_power_state(cstates[target].pwrst,
			      cstates[target].UL_PLL,
			      /* Is actually the AP PLL */
			      cstates[target].UL_PLL);
	/*
	 * If deepsleep/deepidle, Save return address to SRAM and set
	 * this CPU in WFI. This is last core to enter sleep, so we need to
	 * clean both L2 and L1 caches
	 */

	context_save_to_sram_and_wfi(cstates[target].ARM == ARM_OFF);

	t3 = ktime_get();

	/* The PRCMU restores ARM PLL and recouples the GIC */

	context_restore_cpu_registers();

	slept_well = true;
exit:
	restore_sequence(state, slept_well);
	prcmu_disable_wakeups();

exit_fast:
	/* No latency measurements on running, so passing t1 does not matter */
	ux500_ci_dbg_log(CI_RUNNING, t1);

	atomic_dec(&idle_cpus_counter);

	/*
	 * We might have chosen another state than what the
	 * governor recommended
	 */
	if (target != gov_cstate)
		/* Update last state pointer used by CPUIDLE subsystem */
		dev->last_state = &(dev->states[target]);

	t2 = ktime_get();
	diff = ktime_to_us(ktime_sub(t2, t1));
	if (diff > INT_MAX)
		diff = INT_MAX;

	ret = (int)diff;

	ux500_ci_dbg_console_check_uart();
	if (slept_well)
		ux500_ci_dbg_wake_leave(target, t3);

	local_irq_enable();

	ux500_ci_dbg_console();

	return ret;
}

static void init_cstates(struct cpu_state *state)
{
	int i;
	struct cpuidle_state *ci_state;
	struct cpuidle_device *dev;

	dev = &state->dev;
	dev->cpu = state->this_cpu;

	for (i = 0; i < ARRAY_SIZE(cstates); i++) {

		ci_state = &dev->states[i];

		cpuidle_set_statedata(ci_state, (void *)i);

		ci_state->exit_latency = cstates[i].exit_latency;
		ci_state->target_residency = cstates[i].threshold;
		ci_state->flags = cstates[i].flags;
		ci_state->enter = enter_sleep;
		ci_state->power_usage = cstates[i].power_usage;
		snprintf(ci_state->name, CPUIDLE_NAME_LEN, "C%d", i);
		strncpy(ci_state->desc, cstates[i].desc, CPUIDLE_DESC_LEN);
	}

	dev->state_count = ARRAY_SIZE(cstates);

	dev->safe_state = &dev->states[0]; /* Currently not used */

	if (cpuidle_register_device(dev)) {
		printk(KERN_ERR "cpuidle %s: register device failed\n",
		       __func__);
		return;
	}

	pr_debug("cpuidle driver initiated for CPU%d.\n", state->this_cpu);
}

struct cpuidle_driver cpuidle_drv = {
	.name = "cpuidle_driver",
	.owner = THIS_MODULE,
};

static int __init cpuidle_driver_init(void)
{
	int result = 0;
	int cpu;

	if (ux500_is_svp())
		return -ENODEV;

	ux500_ci_dbg_init();

	for_each_possible_cpu(cpu) {
		per_cpu(cpu_state, cpu) = kzalloc(sizeof(struct cpu_state),
						  GFP_KERNEL);
		per_cpu(cpu_state, cpu)->this_cpu = cpu;
	}

	result = cpuidle_register_driver(&cpuidle_drv);
	if (result < 0)
		return result;

	for_each_online_cpu(cpu)
		init_cstates(per_cpu(cpu_state, cpu));


	return result;
}

static void __exit cpuidle_driver_exit(void)
{
	int cpu;
	struct cpuidle_device *dev;

	ux500_ci_dbg_remove();

	for_each_possible_cpu(cpu) {
		dev = &per_cpu(cpu_state, cpu)->dev;
		cpuidle_unregister_device(dev);
	}

	for_each_possible_cpu(cpu)
		kfree(per_cpu(cpu_state, cpu));

	cpuidle_unregister_driver(&cpuidle_drv);
}

module_init(cpuidle_driver_init);
module_exit(cpuidle_driver_exit);

MODULE_DESCRIPTION("U8500 cpuidle driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rickard Andersson <rickard.andersson@stericsson.com>");
