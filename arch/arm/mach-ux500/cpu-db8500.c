/*
 * Copyright (C) 2008-2009 ST-Ericsson SA
 *
 * Author: Srinidhi KASAGAR <srinidhi.kasagar@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/amba/bus.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio/nomadik.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/sys_soc.h>

#include <asm/mach/map.h>
#include <asm/pmu.h>
#include <mach/hardware.h>
#include <mach/setup.h>
#include <mach/devices.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <mach/reboot_reasons.h>
#include <mach/usb.h>
#include <mach/ste-dma40-db8500.h>

#include "devices-db8500.h"

/* minimum static i/o mapping required to boot U8500 platforms */
static struct map_desc u8500_uart_io_desc[] __initdata = {
	__IO_DEV_DESC(U8500_UART0_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_UART2_BASE, SZ_4K),
};

static struct map_desc u8500_io_desc[] __initdata = {
	__IO_DEV_DESC(U8500_GIC_CPU_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_GIC_DIST_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_L2CC_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_TWD_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_MTU0_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_MTU1_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_RTC_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_SCU_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_BACKUPRAM0_BASE, SZ_8K),
	__MEM_DEV_DESC(U8500_BOOT_ROM_BASE, SZ_1M),

	__IO_DEV_DESC(U8500_CLKRST1_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_CLKRST2_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_CLKRST3_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_CLKRST5_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_CLKRST6_BASE, SZ_4K),

	__IO_DEV_DESC(U8500_PRCMU_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_GPIO0_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_GPIO1_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_GPIO2_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_GPIO3_BASE, SZ_4K),
	__IO_DEV_DESC(U8500_PRCMU_TCDM_BASE, SZ_4K),
};

void __init u8500_map_io(void)
{
	/*
	 * Map the UARTs early so that the DEBUG_LL stuff continues to work.
	 */
	iotable_init(u8500_uart_io_desc, ARRAY_SIZE(u8500_uart_io_desc));

	/*
	 * STE NMF CM driver only used on the U8500 allocate using
	 * dma_alloc_coherent:
	 *    8M for SIA and SVA data + 2M for SIA code + 2M for SVA code
	 */
	init_consistent_dma_size(SZ_16M);

	ux500_map_io();

	iotable_init(u8500_io_desc, ARRAY_SIZE(u8500_io_desc));

	_PRCMU_BASE = __io_address(U8500_PRCMU_BASE);
}

static struct resource db8500_pmu_resources[] = {
	[0] = {
		.start		= IRQ_DB8500_PMU,
		.end		= IRQ_DB8500_PMU,
		.flags		= IORESOURCE_IRQ,
	},
};

/*
 * The PMU IRQ lines of two cores are wired together into a single interrupt.
 * Bounce the interrupt to the other core if it's not ours.
 */
static irqreturn_t db8500_pmu_handler(int irq, void *dev, irq_handler_t handler)
{
	irqreturn_t ret = handler(irq, dev);
	int other = !smp_processor_id();

	if (ret == IRQ_NONE && cpu_online(other))
		irq_set_affinity(irq, cpumask_of(other));

	/*
	 * We should be able to get away with the amount of IRQ_NONEs we give,
	 * while still having the spurious IRQ detection code kick in if the
	 * interrupt really starts hitting spuriously.
	 */
	return ret;
}

static struct arm_pmu_platdata db8500_pmu_platdata = {
	.handle_irq		= db8500_pmu_handler,
};

static struct platform_device db8500_pmu_device = {
	.name			= "arm-pmu",
	.id			= ARM_PMU_DEVICE_CPU,
	.num_resources		= ARRAY_SIZE(db8500_pmu_resources),
	.resource		= db8500_pmu_resources,
	.dev.platform_data	= &db8500_pmu_platdata,
};

static struct platform_device db8500_prcmu_device = {
	.name			= "db8500-prcmu",
};

static struct platform_device *platform_devs[] __initdata = {
	&u8500_gpio_devs[0],
	&u8500_gpio_devs[1],
	&u8500_gpio_devs[2],
	&u8500_gpio_devs[3],
	&u8500_gpio_devs[4],
	&u8500_gpio_devs[5],
	&u8500_gpio_devs[6],
	&u8500_gpio_devs[7],
	&u8500_gpio_devs[8],
	&db8500_pmu_device,
	&db8500_prcmu_device,
	&u8500_wdt_device,
};

static int usb_db8500_rx_dma_cfg[] = {
	DB8500_DMA_DEV38_USB_OTG_IEP_1_9,
	DB8500_DMA_DEV37_USB_OTG_IEP_2_10,
	DB8500_DMA_DEV36_USB_OTG_IEP_3_11,
	DB8500_DMA_DEV19_USB_OTG_IEP_4_12,
	DB8500_DMA_DEV18_USB_OTG_IEP_5_13,
	DB8500_DMA_DEV17_USB_OTG_IEP_6_14,
	DB8500_DMA_DEV16_USB_OTG_IEP_7_15,
	DB8500_DMA_DEV39_USB_OTG_IEP_8
};

static int usb_db8500_tx_dma_cfg[] = {
	DB8500_DMA_DEV38_USB_OTG_OEP_1_9,
	DB8500_DMA_DEV37_USB_OTG_OEP_2_10,
	DB8500_DMA_DEV36_USB_OTG_OEP_3_11,
	DB8500_DMA_DEV19_USB_OTG_OEP_4_12,
	DB8500_DMA_DEV18_USB_OTG_OEP_5_13,
	DB8500_DMA_DEV17_USB_OTG_OEP_6_14,
	DB8500_DMA_DEV16_USB_OTG_OEP_7_15,
	DB8500_DMA_DEV39_USB_OTG_OEP_8
};

/*
 * This function is called from the board init
 */
void __init u8500_init_devices(void)
{
#ifdef CONFIG_STM_TRACE
	/* Early init for STM tracing */
	platform_device_register(&u8500_stm_device);
#endif

	db8500_dma_init();
	db8500_add_rtc();
	db8500_add_usb(usb_db8500_rx_dma_cfg, usb_db8500_tx_dma_cfg);

	platform_device_register_simple("cpufreq-u8500", -1, NULL, 0);
	platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));

	return ;
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
			sz += sprintf(buf + sz, "%08x", readl(uid_base + i * sizeof(u32)));
		sz += sprintf(buf + sz, "\n");
	}
	else {
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
		return sprintf(buf, "%d.%d\n" , (rev >> 4) - 0xA + 1, rev & 0xf);

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

static struct sysfs_soc_info soc_info[] = {
	SYSFS_SOC_ATTR_CALLBACK("machine", ux500_get_machine),
	SYSFS_SOC_ATTR_VALUE("family", "Ux500"),
	SYSFS_SOC_ATTR_CALLBACK("soc_id", ux500_get_soc_id),
	SYSFS_SOC_ATTR_CALLBACK("revision", ux500_get_revision),
	SYSFS_SOC_ATTR_CALLBACK("process", ux500_get_process),
	SYSFS_SOC_ATTR_CALLBACK("reset_code", ux500_get_reset_code),
	SYSFS_SOC_ATTR_CALLBACK("reset_reason", ux500_get_reset_reason),
};

static int __init ux500_sys_soc_init(void)
{
	return register_sysfs_soc(soc_info, ARRAY_SIZE(soc_info));
}

module_init(ux500_sys_soc_init);
#endif

