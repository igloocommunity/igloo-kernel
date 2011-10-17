/*
 * Copyright (C) 2009 ST-Ericsson.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <linux/mfd/dbx500-prcmu.h>
#include <mach/reboot_reasons.h>

static inline void arch_idle(void)
{
	/*
	 * This should do all the clock switching
	 * and wait for interrupt tricks
	 */
	cpu_do_idle();
}

static inline void arch_reset(char mode, const char *cmd)
{
#ifdef CONFIG_UX500_SOC_DB8500
	/* Call the PRCMU reset API (w/o reset reason code) */
	prcmu_system_reset(SW_RESET_NO_ARGUMENT);
#endif
}

#endif
