/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 */

#include <linux/kernel.h>
#include <linux/cpufreq.h>

#include <mach/prcmu.h>

#include "cpufreq.h"

static struct cpufreq_frequency_table freq_table[] = {
	[0] = {
		.index = 0,
		.frequency = 200000,
	},
	[1] = {
		.index = 1,
		.frequency = 396500,
	},
	[2] = {
		.index = 2,
		.frequency = 793000,
	},
	[3] = {
		.index = 3,
		.frequency = CPUFREQ_TABLE_END,
	},
};

static enum arm_opp idx2opp[] = {
	ARM_EXTCLK,
	ARM_50_OPP,
	ARM_100_OPP,
};

int u5500_cpufreq_limits(int cpu, int r, unsigned int *min, unsigned int *max)
{
	struct cpufreq_policy p;
	int ret;

	ret = cpufreq_get_policy(&p, cpu);
	if (ret) {
		pr_err("cpufreq-db8500: Failed to get policy.\n");
		return -EINVAL;
	}

	(*max) = p.max;
	(*min) = p.min;

	return 0;
}

static int __init u5500_cpufreq_register(void)
{
	int i = 0;

	BUILD_BUG_ON(ARRAY_SIZE(idx2opp) + 1 != ARRAY_SIZE(freq_table));

	if (cpu_is_u5500v1())
		return -ENODEV;
	pr_info("u5500-cpufreq : Available frequencies:\n");
	while (freq_table[i].frequency != CPUFREQ_TABLE_END)
		pr_info("  %d Mhz\n", freq_table[i++].frequency/1000);

	return ux500_cpufreq_register(freq_table, idx2opp);
}
device_initcall(u5500_cpufreq_register);
