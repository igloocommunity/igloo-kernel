/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for ST-Ericsson
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 */

#ifndef CPUIDLE_DBG_H
#define CPUIDLE_DBG_H

#ifdef CONFIG_U8500_CPUIDLE_DEBUG
void ux500_ci_dbg_init(void);
void ux500_ci_dbg_remove(void);

void ux500_ci_dbg_log(enum ci_pwrst pstate, ktime_t enter_time);
void ux500_ci_dbg_wake_leave(enum ci_pwrst pstate, ktime_t t);

bool ux500_ci_dbg_force_ape_on(void);
int ux500_ci_dbg_deepest_state(void);

void ux500_ci_dbg_console(void);
void ux500_ci_dbg_console_check_uart(void);
void ux500_ci_dbg_console_handle_ape_resume(void);
void ux500_ci_dbg_console_handle_ape_suspend(void);

#ifdef U8500_CPUIDLE_EXTRA_DBG
void ux500_ci_dbg_msg(char *dbg_string);
#else
static inline void ux500_ci_dbg_msg(char *dbg_string) { }
#endif

#else

static inline void ux500_ci_dbg_init(void) { }
static inline void ux500_ci_dbg_remove(void) { }

static inline void ux500_ci_dbg_log(enum ci_pwrst pstate,
				    ktime_t enter_time) { }
static inline void ux500_ci_dbg_wake_leave(enum ci_pwrst pstate, ktime_t t) { }

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
static inline void ux500_ci_dbg_msg(char *dbg_string) { }

#endif
#endif
