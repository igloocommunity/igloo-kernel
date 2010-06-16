/*
 * Copyright (C) 2008-2009 ST-Ericsson
 * Srinidhi Kasagar <srinidhi.kasagar@stericsson.com>
 *
 * This file is heavily based on relaview platform, almost a copy.
 *
 * Copyright (C) 2002 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/clockchips.h>

#include <asm/irq.h>
#include <asm/smp_twd.h>
#include <asm/localtimer.h>

#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
void smp_timer_broadcast(const struct cpumask *mask);
#endif

/*
 * Setup the local clock events for a CPU.
 */
int __cpuinit local_timer_setup(struct clock_event_device *evt)
{
	evt->irq = IRQ_LOCALTIMER;

#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
        evt->broadcast = smp_timer_broadcast;
#endif

	twd_timer_setup(evt);
	return 0;
}
