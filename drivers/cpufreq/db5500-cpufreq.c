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
		.index = 3,
		.frequency = CPUFREQ_TABLE_END,
	},
};

static enum arm_opp idx2opp[] = {
	ARM_EXTCLK,
	ARM_50_OPP,
	ARM_100_OPP
};

/*
 * Below is a temporary workaround for wlan performance issues
 */

#include <linux/kernel_stat.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>

#include <mach/irqs.h>

static struct freq_attr *db5500_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int db5500_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static int db5500_cpufreq_target(struct cpufreq_policy *policy,
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
		pr_err("db5500-cpufreq:  Failed to set OPP level\n");
		return -EINVAL;
	}

	/* post change notification */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

static unsigned int db5500_cpufreq_getspeed(unsigned int cpu)
{
	int i;
	/* request the prcm to get the current ARM opp */
	for (i = 0; prcmu_get_arm_opp() != idx2opp[i]; i++)
		;
	return freq_table[i].frequency;
}

static int __cpuinit db5500_cpufreq_init(struct cpufreq_policy *policy)
{
	int res;
	int i = 0;

	BUILD_BUG_ON(ARRAY_SIZE(idx2opp) + 1 != ARRAY_SIZE(freq_table));

	pr_info("db5500-cpufreq : Available frequencies:\n");
	while (freq_table[i].frequency != CPUFREQ_TABLE_END)
		pr_info("  %d Mhz\n", freq_table[i++].frequency/1000);

	/* get policy fields based on the table */
	res = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (!res)
		cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	else {
		pr_err("db5500-cpufreq : Failed to read policy table\n");
		return res;
	}

	policy->min = policy->cpuinfo.min_freq;
	policy->max = policy->cpuinfo.max_freq;
	policy->cur = db5500_cpufreq_getspeed(policy->cpu);

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

static struct cpufreq_driver db5500_cpufreq_driver = {
	.flags  = CPUFREQ_STICKY,
	.verify = db5500_cpufreq_verify_speed,
	.target = db5500_cpufreq_target,
	.get    = db5500_cpufreq_getspeed,
	.init   = db5500_cpufreq_init,
	.name   = "DB5500",
	.attr   = db5500_cpufreq_attr,
};

static int __init db5500_cpufreq_register(void)
{
	pr_info("cpufreq for DB5500 started\n");
	return cpufreq_register_driver(&db5500_cpufreq_driver);
}
device_initcall(db5500_cpufreq_register);
