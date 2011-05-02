/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __MACH_SUSPEND_H
#define __MACH_SUSPEND_H

void suspend_block_sleep(void);
void suspend_unblock_sleep(void);
void suspend_set_pins_force_fn(void (*force)(void), void (*force_mux)(void));

#endif /* __MACH_SUSPEND_H */
