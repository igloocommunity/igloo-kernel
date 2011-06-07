/*
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for ST-Ericsson
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 */

#ifndef CPUIDLE_DBG_H
#define CPUIDLE_DBG_H

#include "cpuidle.h"

#ifdef CONFIG_U8500_CPUIDLE_DEBUG
void ux500_ci_dbg_init(void);
void ux500_ci_dbg_remove(void);

void ux500_ci_dbg_log(int ctarget, ktime_t enter_time);
void ux500_ci_dbg_wake_latency(int ctarget, int sleep_time);
void ux500_ci_dbg_exit_latency(int ctarget, ktime_t now, ktime_t exit,
			       ktime_t enter);

void ux500_ci_dbg_register_reason(int idx, bool power_state_req,
				  u32 sleep_time, u32 max_depth);

bool ux500_ci_dbg_force_ape_on(void);
int ux500_ci_dbg_deepest_state(void);

void ux500_ci_dbg_console(void);
void ux500_ci_dbg_console_check_uart(void);
void ux500_ci_dbg_console_handle_ape_resume(void);
void ux500_ci_dbg_console_handle_ape_suspend(void);

#else

static inline void ux500_ci_dbg_init(void) { }
static inline void ux500_ci_dbg_remove(void) { }

static inline void ux500_ci_dbg_log(int ctarget,
				    ktime_t enter_time) { }

static inline void ux500_ci_dbg_exit_latency(int ctarget,
					     ktime_t now, ktime_t exit,
					     ktime_t enter) { }
static inline void ux500_ci_dbg_wake_latency(int ctarget, int sleep_time) { }


static inline void ux500_ci_dbg_register_reason(int idx, bool power_state_req,
						u32 sleep_time, u32 max_depth) { }

static inline bool ux500_ci_dbg_force_ape_on(void)
{
	return false;
}

static inline int ux500_ci_dbg_deepest_state(void)
{
	/* This means no lower sleep state than ApIdle */
	return CONFIG_U8500_CPUIDLE_DEEPEST_STATE;
}

static inline void ux500_ci_dbg_console(void) { }
static inline void ux500_ci_dbg_console_check_uart(void) { }
static inline void ux500_ci_dbg_console_handle_ape_resume(void) { }
static inline void ux500_ci_dbg_console_handle_ape_suspend(void) { }

#endif
#endif
