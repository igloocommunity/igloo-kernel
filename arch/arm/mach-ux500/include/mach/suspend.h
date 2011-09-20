/*
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __MACH_SUSPEND_H
#define __MACH_SUSPEND_H

#ifdef CONFIG_UX500_SUSPEND
void suspend_block_sleep(void);
void suspend_unblock_sleep(void);
#else
static inline void suspend_block_sleep(void) { }
static inline void suspend_unblock_sleep(void) { }
#endif

#endif /* __MACH_SUSPEND_H */
