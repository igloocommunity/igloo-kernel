/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/mfd/db8500-prcmu.h>
#include <linux/mfd/db5500-prcmu.h>
#include <linux/clksrc-dbx500-prcmu.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/sys_soc.h>

#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/localtimer.h>

#include <mach/hardware.h>
#include <mach/setup.h>
#include <mach/devices.h>
#include <mach/reboot_reasons.h>
#include <mach/pm.h>

#include "clock.h"

void __iomem *_PRCMU_BASE;

void ux500_restart(char mode, const char *cmd)
{
	unsigned short reset_code;

	reset_code = reboot_reason_code(cmd);
	prcmu_system_reset(reset_code);

	mdelay(1000);

	/*
	 * On 5500, the PRCMU firmware waits for up to 2 seconds for the modem
	 * to respond.
	 */
	if (cpu_is_u5500())
		mdelay(2000);

	printk(KERN_ERR "Reboot via PRCMU failed -- System halted\n");
	while (1)
		;
}

void __init ux500_init_irq(void)
{
	void __iomem *dist_base;
	void __iomem *cpu_base;

	if (cpu_is_u5500()) {
		dist_base = __io_address(U5500_GIC_DIST_BASE);
		cpu_base = __io_address(U5500_GIC_CPU_BASE);
	} else if (cpu_is_u8500() || cpu_is_u9540()) {
		dist_base = __io_address(U8500_GIC_DIST_BASE);
		cpu_base = __io_address(U8500_GIC_CPU_BASE);
	} else
		ux500_unknown_soc();

	gic_init(0, 29, dist_base, cpu_base);

	/*
	 * On WD reboot gic is in some cases decoupled.
	 * This will make sure that the GIC is correctly configured.
	 */
	ux500_pm_gic_recouple();

	/*
	 * Init clocks here so that they are available for system timer
	 * initialization.
	 */
	if (cpu_is_u5500())
		db5500_prcmu_early_init();
	if (cpu_is_u8500() || cpu_is_u9540())
		db8500_prcmu_early_init();

	arm_pm_restart = ux500_restart;
	clk_init();
}

#ifdef CONFIG_SYS_SOC
#define U8500_BB_UID_BASE (U8500_BACKUPRAM1_BASE + 0xFC0)
#define U8500_BB_UID_LENGTH 5

static ssize_t ux500_get_machine(char *buf, struct sysfs_soc_info *si)
{
	return sprintf(buf, "DB%2x00\n", dbx500_id.partnumber);
}

static ssize_t ux500_get_soc_id(char *buf, struct sysfs_soc_info *si)
{
	void __iomem *uid_base;
	int i;
	ssize_t sz = 0;

	if (dbx500_id.partnumber == 0x85) {
		uid_base = __io_address(U8500_BB_UID_BASE);
		for (i = 0; i < U8500_BB_UID_LENGTH; i++)
			sz += sprintf(buf + sz, "%08x",
					readl(uid_base + i * sizeof(u32)));
		sz += sprintf(buf + sz, "\n");
	} else {
		/* Don't know where it is located for U5500 */
		sz = sprintf(buf, "N/A\n");
	}

	return sz;
}

static ssize_t ux500_get_revision(char *buf, struct sysfs_soc_info *si)
{
	unsigned int rev = dbx500_id.revision;

	if (rev == 0x01)
		return sprintf(buf, "%s\n", "ED");
	else if (rev >= 0xA0)
		return sprintf(buf, "%d.%d\n" ,
				(rev >> 4) - 0xA + 1, rev & 0xf);

	return sprintf(buf, "%s", "Unknown\n");
}

static ssize_t ux500_get_process(char *buf, struct sysfs_soc_info *si)
{
	if (dbx500_id.process == 0x00)
		return sprintf(buf, "Standard\n");

	return sprintf(buf, "%02xnm\n", dbx500_id.process);
}

static ssize_t ux500_get_reset_code(char *buf, struct sysfs_soc_info *si)
{
	return sprintf(buf, "0x%04x\n", prcmu_get_reset_code());
}

static ssize_t ux500_get_reset_reason(char *buf, struct sysfs_soc_info *si)
{
	return sprintf(buf, "%s\n",
		reboot_reason_string(prcmu_get_reset_code()));
}

static ssize_t ux500_get_reset_status(char *buf, struct sysfs_soc_info *si)
{
	return sprintf(buf, "0x%08x\n", prcmu_get_reset_status());
}

static struct sysfs_soc_info soc_info[] = {
	SYSFS_SOC_ATTR_CALLBACK("machine", ux500_get_machine),
	SYSFS_SOC_ATTR_VALUE("family", "Ux500"),
	SYSFS_SOC_ATTR_CALLBACK("soc_id", ux500_get_soc_id),
	SYSFS_SOC_ATTR_CALLBACK("revision", ux500_get_revision),
	SYSFS_SOC_ATTR_CALLBACK("process", ux500_get_process),
	SYSFS_SOC_ATTR_CALLBACK("reset_code", ux500_get_reset_code),
	SYSFS_SOC_ATTR_CALLBACK("reset_reason", ux500_get_reset_reason),
	SYSFS_SOC_ATTR_CALLBACK("reset_status", ux500_get_reset_status),
};

static int __init ux500_sys_soc_init(void)
{
	return register_sysfs_soc(soc_info, ARRAY_SIZE(soc_info));
}

module_init(ux500_sys_soc_init);
#endif
