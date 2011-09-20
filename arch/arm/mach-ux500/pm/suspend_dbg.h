/*
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 *
 */

#ifndef UX500_SUSPEND_DBG_H
#define UX500_SUSPEND_DBG_H

#ifdef CONFIG_UX500_SUSPEND_DBG_WAKE_ON_UART
void ux500_suspend_dbg_add_wake_on_uart(void);
void ux500_suspend_dbg_remove_wake_on_uart(void);
#else
static inline void ux500_suspend_dbg_add_wake_on_uart(void) { }
static inline void ux500_suspend_dbg_remove_wake_on_uart(void) { }
#endif

#ifdef CONFIG_UX500_SUSPEND_DBG
void ux500_suspend_dbg_sleep_status(bool is_deepsleep);
#else
static inline void ux500_suspend_dbg_sleep_status(bool is_deepsleep) { }
#endif

#endif
