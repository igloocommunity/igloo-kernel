/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for
 *         ST-Ericsson. Loosly based on cpuidle.c by Sundar Iyer.
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#ifndef __CPUIDLE_H
#define __CPUIDLE_H

#include <linux/cpuidle.h>

#include <mach/prcmu-fw-defs_v1.h>

enum ARM {
	ARM_OFF,
	ARM_RET,
	ARM_ON
};

enum APE {
	APE_OFF,
	APE_ON
};

enum ARM_PLL {
	ARM_PLL_OFF = 0,
	ARM_PLL_ON = 1
};

enum UL_PLL {
	UL_PLL_OFF,
	UL_PLL_ON
};

enum ESRAM {
	ESRAM_OFF,
	ESRAM_RET
};

enum ci_pwrst {
	CI_RUNNING = 0,
	CI_WFI = 1,
	CI_IDLE,
	CI_SLEEP,
	CI_DEEP_IDLE,
	CI_DEEP_SLEEP,
};

struct cstate {
	/* Required state of different hardwares */
	enum ARM ARM;
	enum APE APE;
	enum ARM_PLL ARM_PLL;
	enum UL_PLL UL_PLL;
	/* ESRAM = ESRAM_RET means that ESRAM context to be kept */
	enum ESRAM ESRAM;

	u32 enter_latency;
	u32 exit_latency;
	u32 power_usage;
	u32 threshold;
	u32 flags;
	enum ap_pwrst_trans pwrst;

	/* Only used for debugging purpose */
	enum ci_pwrst state;
	char desc[CPUIDLE_DESC_LEN];
};

struct cstate *ux500_ci_get_cstates(int *len);

#ifdef CONFIG_U8500_CPUIDLE
void ux500_cpuidle_plug(int cpu);
void ux500_cpuidle_unplug(int cpu);
#else
static inline void ux500_cpuidle_plug(int cpu)
{
}

static inline void ux500_cpuidle_unplug(int cpu)
{
}
#endif

#endif
