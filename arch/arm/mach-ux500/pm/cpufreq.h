/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 */
#ifndef __UX500_PM_CPUFREQ_H
#define __UX500_PM_CPUFREQ_H

#include <linux/cpufreq.h>

extern int ux500_cpufreq_register(struct cpufreq_frequency_table *freq_table,
				  enum arm_opp *idx2opp);

/* This function lives in drivers/cpufreq/cpufreq.c */
int cpufreq_update_freq(int cpu, unsigned int min, unsigned int max);

int ux500_cpufreq_limits(int cpu, int r, unsigned int *min, unsigned int *max);

#endif
