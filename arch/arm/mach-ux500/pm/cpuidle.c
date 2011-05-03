/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson.
 *
 * Loosely based on cpuidle.c by Sundar Iyer.
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/clockchips.h>

#include <linux/gpio.h>

#include <mach/prcmu-fw-api.h>

#include "cpuidle.h"
#include "cpuidle_dbg.h"
#include "context.h"
#include "pm.h"
#include "timer.h"
#include "../regulator-db8500.h"

#define DEEP_SLEEP_WAKE_UP_LATENCY 8500
/* Exit latency from ApSleep is measured to be around 1.0 to 1.5 ms */
#define MIN_SLEEP_WAKE_UP_LATENCY 1000
#define MAX_SLEEP_WAKE_UP_LATENCY 1500
#define UL_PLL_START_UP_LATENCY 8000 /* us */

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
		.enter_latency = 40,
		.exit_latency = 50,
		.threshold = 150,
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
		.enter_latency = 45,
		.exit_latency = 50,
		.threshold = 160,
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
		.enter_latency = 120,
		.exit_latency = MAX_SLEEP_WAKE_UP_LATENCY,
		/*
		 * Note: Sleep time must be longer than 120 us or else
		 * there might be issues with the RTC-RTT block.
		 */
		.threshold = MAX_SLEEP_WAKE_UP_LATENCY + 120 + 200,
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
		.enter_latency = 150,
		.exit_latency = (MAX_SLEEP_WAKE_UP_LATENCY +
				   UL_PLL_START_UP_LATENCY),
		.threshold = (2 * (MAX_SLEEP_WAKE_UP_LATENCY +
				   UL_PLL_START_UP_LATENCY + 200)),
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
		.enter_latency = 160,
		.exit_latency = DEEP_SLEEP_WAKE_UP_LATENCY,
		.threshold = DEEP_SLEEP_WAKE_UP_LATENCY + 160 + 50,
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
		.enter_latency = 200,
		.exit_latency = DEEP_SLEEP_WAKE_UP_LATENCY,
		.threshold = DEEP_SLEEP_WAKE_UP_LATENCY + 200 + 50,
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
	int gov_cstate;
	ktime_t sched_wake_up;
	struct cpuidle_device dev;
};

static DEFINE_PER_CPU(struct cpu_state, *cpu_state);

static DEFINE_SPINLOCK(cpuidle_lock);
static bool restore_ape; /* protected by cpuidle_lock */
static bool restore_arm; /* protected by cpuidle_lock */
static ktime_t time_next;  /* protected by cpuidle_lock */

extern struct clock_event_device u8500_mtu_clkevt;

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

static void restore_sequence(ktime_t now)
{
	int this_cpu = smp_processor_id();

	spin_lock(&cpuidle_lock);

	smp_rmb();
	if (restore_arm) {

		restore_arm = false;
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

		ux500_rtcrtt_off();

		/*
		 * If we're returning from ApSleep and the RTC timer
		 * caused the wake up, program the MTU to trigger.
		 */
		if ((ktime_to_us(now) > ktime_to_us(time_next)))
			time_next = ktime_add(now, ktime_set(0, 1000));

		/* Make sure have an MTU interrupt waiting for us */
		clockevents_program_event(&u8500_mtu_clkevt,
					  time_next,
					  now);
	}

	spin_unlock(&cpuidle_lock);

	/* Use the ARM local timer for this cpu */
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT,
			   &this_cpu);

}

/**
 * get_remaining_sleep_time() - returns remaining sleep time in
 * microseconds (us)
 */
static int get_remaining_sleep_time(ktime_t *next, int *on_cpu)
{
	ktime_t now, t;
	int cpu;
	int delta;
	int remaining_sleep_time = INT_MAX;

	now = ktime_get();

	/* Check next schedule to expire considering both cpus */

	spin_lock(&cpuidle_lock);
	for_each_online_cpu(cpu) {
		t = per_cpu(cpu_state, cpu)->sched_wake_up;

		delta = ktime_to_us(ktime_sub(t, now));
		if ((delta < remaining_sleep_time) && (delta > 0)) {
			remaining_sleep_time = delta;
			if (next)
				(*next) = t;
			if (on_cpu)
				(*on_cpu) = cpu;
		}
	}
	spin_unlock(&cpuidle_lock);

	return remaining_sleep_time;
}

static int determine_sleep_state(void)
{
	int i;
	int sleep_time;
	int cpu;
	int max_depth;
	bool power_state_req;

	/* If first cpu to sleep, go to most shallow sleep state */
	if (atomic_read(&idle_cpus_counter) < num_online_cpus())
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

	sleep_time = get_remaining_sleep_time(NULL, NULL);

	/*
	 * Never go deeper than the governor recommends even though it might be
	 * possible from a scheduled wake up point of view
	 */
	max_depth = ux500_ci_dbg_deepest_state();

	for_each_online_cpu(cpu) {
		if (max_depth > per_cpu(cpu_state, cpu)->gov_cstate)
			max_depth = per_cpu(cpu_state, cpu)->gov_cstate;
	}

	for (i = max_depth; i > 0; i--) {

		if (sleep_time <= cstates[i].threshold)
			continue;

		if (cstates[i].APE == APE_OFF) {
			/* This state says APE should be off */
			if (power_state_req ||
			    ux500_ci_dbg_force_ape_on())
				continue;
		}

		/* OK state */
		break;
	}

	return max(CI_WFI, i);
}

static void enter_sleep_shallow(struct cpu_state *state, ktime_t t1)
{

	switch (cstates[state->gov_cstate].ARM) {
	case ARM_OFF:
		context_varm_save_core();
		context_save_cpu_registers();
		/* fall through */
	case ARM_RET:
		ux500_ci_dbg_log(CI_WFI, t1);
		context_save_to_sram_and_wfi(false);
		if (cstates[state->gov_cstate].ARM == ARM_OFF) {
			context_restore_cpu_registers();
			context_varm_restore_core();
		}
		break;
	case ARM_ON:
		ux500_ci_dbg_log(CI_WFI, t1);
		__asm__ __volatile__
			("dsb\n\t" "wfi\n\t" : : : "memory");
		break;
	default:
		break;
	}
}

static int enter_sleep(struct cpuidle_device *dev,
		       struct cpuidle_state *ci_state)
{
	ktime_t time_enter, time_exit, time_wake;
	ktime_t wake_up;
	u32 sleep_time;
	s64 diff;
	int ret;
	int target;
	struct cpu_state *state;
	u32 divps_rate;
	bool slept_well = false;
	int this_cpu = smp_processor_id();

	local_irq_disable();

	time_enter = ktime_get(); /* Time now */

	state = per_cpu(cpu_state, smp_processor_id());

	wake_up = ktime_add(time_enter, tick_nohz_get_sleep_length());

	spin_lock(&cpuidle_lock);

	/* Save scheduled wake up for this cpu */
	state->sched_wake_up = wake_up;

	/* Retrive the cstate that the governor recommends for this CPU */
	state->gov_cstate = (int) cpuidle_get_statedata(ci_state);

	spin_unlock(&cpuidle_lock);

	atomic_inc(&idle_cpus_counter);

	/*
	 * Determine sleep state considering both CPUs and
	 * shared resources like e.g. VAPE
	 */
	target = determine_sleep_state();
	if (target < 0)
		/* "target" will be last_state in the cpuidle framework */
		goto exit_fast;

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &this_cpu);

	if (cstates[target].ARM == ARM_ON) {
		/* Handle first cpu to enter sleep state */
		enter_sleep_shallow(state, time_enter);
		time_wake = ktime_get();
		slept_well = true;
		goto exit;
	}

	/* Decouple GIC from the interrupt bus */
	ux500_pm_gic_decouple();

	if (!ux500_pm_other_cpu_wfi())
		/* Other CPU was not in WFI => abort */
		goto exit;

	/*
	 * Check if we have a pending interrupt or if sleep
	 * state has changed after GIC has been frozen
	 */
	if (ux500_pm_gic_pending_interrupt())
		goto exit;

	/* Copy GIC interrupt settings to PRCMU interrupt settings */
	ux500_pm_prcmu_copy_gic_settings();

	/* Clean the cache before slowing down cpu frequency */
	if (cstates[target].ARM == ARM_OFF)
		context_clean_l1_cache_all();

	divps_rate = ux500_pm_arm_on_ext_clk(cstates[target].ARM_PLL);

	if (ux500_pm_prcmu_pending_interrupt()) {
		/* An interrupt found => abort */
		ux500_pm_arm_on_arm_pll(divps_rate);
		goto exit;
	}

	/* No PRCMU interrupt was pending => continue the sleeping stages */

	if (cstates[target].APE == APE_OFF) {
		ktime_t est_wake_time;
		int wake_cpu;

		/* We are going to sleep or deep sleep => prepare for it */

		/* Program the only timer that is available when APE is off */

		sleep_time = get_remaining_sleep_time(&est_wake_time,
						      &wake_cpu);

		if (sleep_time == INT_MAX)
			goto exit;

		if (cstates[target].UL_PLL == UL_PLL_OFF)
			/* Compensate for ULPLL start up time */
			sleep_time -= UL_PLL_START_UP_LATENCY;

			/*
			 * Not checking for negative sleep time since
			 * determine_sleep_state has already checked that
			 * there is enough time.
			 */

		/* Adjust for exit latency */
		sleep_time -= MIN_SLEEP_WAKE_UP_LATENCY;

		ux500_rtcrtt_next(sleep_time);

		/*
		 * Make sure the cpu that is scheduled first gets
		 * the prcmu interrupt.
		 */
		irq_set_affinity(IRQ_DB8500_PRCMU1, cpumask_of(wake_cpu));

		context_vape_save();

		ux500_ci_dbg_console_handle_ape_suspend();
		ux500_pm_prcmu_set_ioforce(true);

		spin_lock(&cpuidle_lock);
		restore_ape = true;
		time_next = est_wake_time;
		spin_unlock(&cpuidle_lock);
	}

	if (cstates[target].ARM == ARM_OFF) {
		/* Save gic settings */
		context_varm_save_common();

		context_varm_save_core();

		spin_lock(&cpuidle_lock);
		restore_arm = true;
		spin_unlock(&cpuidle_lock);
		context_save_cpu_registers();
	}

	/* TODO: To use desc as debug print might be a bad idea */
	ux500_ci_dbg_msg(cstates[target].desc);

	/*
	 * Due to we have only 100us between requesting a
	 * powerstate and wfi, we clean the cache before as
	 * well to assure the final cache clean before wfi
	 * has as little as possible to do.
	 */
	if (cstates[target].ARM == ARM_OFF)
		context_clean_l1_cache_all();

	ux500_ci_dbg_log(target, time_enter);

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

	ux500_ci_dbg_wake_latency(target);

	time_wake = ktime_get();

	/* The PRCMU restores ARM PLL and recouples the GIC */
	if (cstates[target].ARM == ARM_OFF) {
		context_restore_cpu_registers();
		context_varm_restore_core();
	}

	slept_well = true;
exit:
	if (!slept_well)
		/* Recouple GIC with the interrupt bus */
		ux500_pm_gic_recouple();

	restore_sequence(time_wake);

exit_fast:

	if (target < 0)
		target = CI_RUNNING;

	/* 16 minutes ahead */
	wake_up = ktime_add_us(time_enter,
			       1000000000);

	spin_lock(&cpuidle_lock);
	/* Remove wake up time i.e. set wake up far ahead */
	state->sched_wake_up = wake_up;
	spin_unlock(&cpuidle_lock);

	atomic_dec(&idle_cpus_counter);

	/*
	 * We might have chosen another state than what the
	 * governor recommended
	 */
	if (target != state->gov_cstate)
		/* Update last state pointer used by CPUIDLE subsystem */
		dev->last_state = &(dev->states[target]);

	time_exit = ktime_get();
	diff = ktime_to_us(ktime_sub(time_exit, time_enter));
	if (diff > INT_MAX)
		diff = INT_MAX;

	ret = (int)diff;

	ux500_ci_dbg_console_check_uart();
	if (slept_well) {
		bool timed_out;

		spin_lock(&cpuidle_lock);
		timed_out = ktime_to_us(time_wake) > ktime_to_us(time_next);
		spin_unlock(&cpuidle_lock);

		ux500_ci_dbg_exit_latency(target,
					  time_exit, /* now */
					  time_wake, /* exit from wfi */
					  time_enter, /* enter cpuidle */
					  timed_out);
	}

	ux500_ci_dbg_log(CI_RUNNING, time_exit);

	local_irq_enable();

	ux500_ci_dbg_console();

	return ret;
}

static int init_cstates(int cpu, struct cpu_state *state)
{
	int i;
	struct cpuidle_state *ci_state;
	struct cpuidle_device *dev;

	dev = &state->dev;
	dev->cpu = cpu;

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

	return cpuidle_register_device(dev);
}

struct cpuidle_driver cpuidle_drv = {
	.name = "cpuidle_driver",
	.owner = THIS_MODULE,
};

static int __init cpuidle_driver_init(void)
{
	int res = -ENODEV;
	int cpu;

	if (ux500_is_svp())
		goto out;

	/* Configure wake up reasons */
	prcmu_enable_wakeups(PRCMU_WAKEUP(ARM) | PRCMU_WAKEUP(RTC) |
			     PRCMU_WAKEUP(ABB));

	ux500_ci_dbg_init();

	for_each_possible_cpu(cpu)
		per_cpu(cpu_state, cpu) = kzalloc(sizeof(struct cpu_state),
						  GFP_KERNEL);

	res = cpuidle_register_driver(&cpuidle_drv);
	if (res)
		goto out;

	for_each_possible_cpu(cpu) {
		res = init_cstates(cpu, per_cpu(cpu_state, cpu));
		if (res)
			goto out;
		pr_info("cpuidle: initiated for CPU%d.\n", cpu);
	}
	return 0;
out:
	pr_err("cpuidle: initialization failed.\n");
	return res;
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
