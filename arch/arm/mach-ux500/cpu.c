/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/mfd/db8500-prcmu.h>
#include <linux/mfd/db5500-prcmu.h>
#include <linux/clksrc-dbx500-prcmu.h>
#include <linux/delay.h>
#include <linux/mfd/dbx500-prcmu.h>

#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/localtimer.h>

#include <mach/hardware.h>
#include <mach/setup.h>
#include <mach/devices.h>
#include <mach/reboot_reasons.h>

#include "clock.h"

void __iomem *_PRCMU_BASE;

static void ux500_restart(char mode, const char *cmd)
{
	unsigned short reset_code;

	reset_code = reboot_reason_code(cmd);
	prcmu_system_reset(reset_code);

	mdelay(1000);
	printk("Reboot via PRCMU failed -- System halted\n");
	while (1);
}

void __init ux500_init_irq(void)
{
	void __iomem *dist_base;
	void __iomem *cpu_base;

	if (cpu_is_u5500()) {
		dist_base = __io_address(U5500_GIC_DIST_BASE);
		cpu_base = __io_address(U5500_GIC_CPU_BASE);
	} else if (cpu_is_u8500()) {
		dist_base = __io_address(U8500_GIC_DIST_BASE);
		cpu_base = __io_address(U8500_GIC_CPU_BASE);
	} else
		ux500_unknown_soc();

	gic_init(0, 29, dist_base, cpu_base);

	/*
	 * Init clocks here so that they are available for system timer
	 * initialization.
	 */
	if (cpu_is_u5500())
		db5500_prcmu_early_init();
	if (cpu_is_u8500())
		db8500_prcmu_early_init();

	arm_pm_restart = ux500_restart;
	clk_init();
}
