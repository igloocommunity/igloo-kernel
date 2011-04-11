/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Sundar Iyer <sundar.iyer@stericsson.com>
 * Author: Martin Persson <martin.persson@stericsson.com>
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com>
 *
 */
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mach/prcmu.h>
#include <mach/id.h>

static struct cpufreq_frequency_table freq_table[] = {
	[0] = {
		.index = 0,
		.frequency = 200000,
	},
	[1] = {
		.index = 1,
		.frequency = 300000,
	},
	[2] = {
		.index = 2,
		.frequency = 600000,
	},
	[3] = {
		/* Used for MAX_OPP, if available */
		.index = 3,
		.frequency = CPUFREQ_TABLE_END,
	},
	[4] = {
		.index = 4,
		.frequency = CPUFREQ_TABLE_END,
	},
};

static enum arm_opp idx2opp[] = {
	ARM_EXTCLK,
	ARM_50_OPP,
	ARM_100_OPP,
	ARM_MAX_OPP
};

/*
 * Below is a temporary workaround for wlan performance issues
 */

#include <linux/kernel_stat.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>

#include <mach/irqs.h>

#define WLAN_PROBE_DELAY 3000 /* 3 seconds */
#define WLAN_LIMIT (3000/3) /* If we have more than 1000 irqs per second */
#define USB_PROBE_DELAY 1000 /* 1 seconds */
#define USB_LIMIT (200) /* If we have more than 200 irqs per second */
static struct delayed_work work_usb_workaround;
bool usb_mode_on;

static struct delayed_work work_wlan_workaround;
bool wlan_mode_on;

static void wlan_load(struct work_struct *work)
{
	int cpu;
	unsigned int num_irqs = 0;
	static unsigned int old_num_irqs = UINT_MAX;

	for_each_online_cpu(cpu)
		num_irqs += kstat_irqs_cpu(IRQ_DB8500_SDMMC1, cpu);

	if ((num_irqs > old_num_irqs) &&
	    (num_irqs - old_num_irqs) > WLAN_LIMIT)
		wlan_mode_on = true;
	else
		wlan_mode_on = false;

	old_num_irqs = num_irqs;

	schedule_delayed_work_on(0,
				 &work_wlan_workaround,
				 msecs_to_jiffies(WLAN_PROBE_DELAY));
}

static void usb_load(struct work_struct *work)
{
	int cpu;
	unsigned int num_irqs = 0;
	static unsigned int old_num_irqs = UINT_MAX;

	for_each_online_cpu(cpu)
		num_irqs += kstat_irqs_cpu(IRQ_DB8500_USBOTG, cpu);

	if ((num_irqs > old_num_irqs) &&
	    (num_irqs - old_num_irqs) > USB_LIMIT)
		usb_mode_on = true;
	else
		usb_mode_on = false;

	old_num_irqs = num_irqs;

	schedule_delayed_work_on(0,
				 &work_usb_workaround,
				 msecs_to_jiffies(USB_PROBE_DELAY));
}

void cpufreq_usb_connect_notify(bool connect)
{
	if (connect) {
		schedule_delayed_work_on(0,
				 &work_usb_workaround,
				 msecs_to_jiffies(USB_PROBE_DELAY));
	} else {
		cancel_delayed_work_sync(&work_usb_workaround);
		usb_mode_on = false;
	}
}

static struct freq_attr *db8500_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int db8500_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static int db8500_cpufreq_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
	struct cpufreq_freqs freqs;
	unsigned int idx;

	/* scale the target frequency to one of the extremes supported */
	if (target_freq < policy->cpuinfo.min_freq)
		target_freq = policy->cpuinfo.min_freq;
	if (target_freq > policy->cpuinfo.max_freq)
		target_freq = policy->cpuinfo.max_freq;

	/* Lookup the next frequency */
	if (cpufreq_frequency_table_target
	    (policy, freq_table, target_freq, relation, &idx)) {
		return -EINVAL;
	}

	freqs.old = policy->cur;
	freqs.new = freq_table[idx].frequency;
	freqs.cpu = policy->cpu;

	if (freqs.old == freqs.new)
		return 0;

	/* pre-change notification */
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/* request the PRCM unit for opp change */
	if (prcmu_set_arm_opp(idx2opp[idx])) {
		pr_err("db8500-cpufreq:  Failed to set OPP level\n");
		return -EINVAL;
	}

	/* post change notification */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

static unsigned int db8500_cpufreq_getspeed(unsigned int cpu)
{
	int i;
	/* request the prcm to get the current ARM opp */
	for (i = 0; prcmu_get_arm_opp() != idx2opp[i]; i++)
		;
	return freq_table[i].frequency;
}

static int __cpuinit db8500_cpufreq_init(struct cpufreq_policy *policy)
{
	int res;
	int i = 0;

	BUILD_BUG_ON(ARRAY_SIZE(idx2opp) + 1 != ARRAY_SIZE(freq_table));

	if (!prcmu_is_u8400()) {
		freq_table[1].frequency = 400000;
		freq_table[2].frequency = 800000;
		if (prcmu_has_arm_maxopp())
			freq_table[3].frequency = 1000000;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&work_wlan_workaround,
				     wlan_load);

	schedule_delayed_work_on(0,
				 &work_wlan_workaround,
				 msecs_to_jiffies(WLAN_PROBE_DELAY));

	INIT_DELAYED_WORK_DEFERRABLE(&work_usb_workaround,
				     usb_load);

	pr_info("db8500-cpufreq : Available frequencies:\n");
	while (freq_table[i].frequency != CPUFREQ_TABLE_END)
		pr_info("  %d Mhz\n", freq_table[i++].frequency/1000);

	/* get policy fields based on the table */
	res = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (!res)
		cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	else {
		pr_err("db8500-cpufreq : Failed to read policy table\n");
		return res;
	}

	policy->min = policy->cpuinfo.min_freq;
	policy->max = policy->cpuinfo.max_freq;
	policy->cur = db8500_cpufreq_getspeed(policy->cpu);

	for (i = 0; freq_table[i].frequency != policy->cur; i++)
		;

	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	/*
	 * FIXME : Need to take time measurement across the target()
	 *	   function with no/some/all drivers in the notification
	 *	   list.
	 */
	policy->cpuinfo.transition_latency = 20 * 1000; /* in ns */

	/* policy sharing between dual CPUs */
	cpumask_copy(policy->cpus, &cpu_present_map);

	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;

	return 0;
}

static struct cpufreq_driver db8500_cpufreq_driver = {
	.flags  = CPUFREQ_STICKY,
	.verify = db8500_cpufreq_verify_speed,
	.target = db8500_cpufreq_target,
	.get    = db8500_cpufreq_getspeed,
	.init   = db8500_cpufreq_init,
	.name   = "DB8500",
	.attr   = db8500_cpufreq_attr,
};

static int __init db8500_cpufreq_register(void)
{
	if (!cpu_is_u8500v20_or_later())
		return -ENODEV;

	pr_info("cpufreq for DB8500 started\n");
	return cpufreq_register_driver(&db8500_cpufreq_driver);
}
device_initcall(db8500_cpufreq_register);
