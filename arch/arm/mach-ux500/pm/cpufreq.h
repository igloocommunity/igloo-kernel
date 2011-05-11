/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 */
#ifndef __UX500_PM_CPUFREQ_H
#define __UX500_PM_CPUFREQ_H

extern int ux500_cpufreq_register(struct cpufreq_frequency_table *freq_table,
				  enum arm_opp *idx2opp);

#endif
