/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 */
#ifndef __UX500_PM_CPUFREQ_DBX500_H
#define __UX500_PM_CPUFREQ_DBX500_H

int u8500_cpufreq_limits(int cpu, int r, unsigned int *min, unsigned int *max);

int u5500_cpufreq_limits(int cpu, int r, unsigned int *min, unsigned int *max);

#endif
