/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/amba/bus.h>

#include <mach/crypto-ux500.h>
#include <mach/hardware.h>
#include <mach/setup.h>

static struct resource ux500_hash1_resources[] = {
	[0] = {
		.start = U8500_HASH1_BASE,
		.end = U8500_HASH1_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device ux500_hash1_device = {
	.name = "hash1",
	.id = -1,
	.num_resources = 1,
	.resource = ux500_hash1_resources
};

static struct resource ux500_cryp1_resources[] = {
	[0] = {
		.start = U8500_CRYP1_BASE,
		.end = U8500_CRYP1_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_DB8500_CRYP1,
		.end = IRQ_DB8500_CRYP1,
		.flags = IORESOURCE_IRQ
	}
};

static struct cryp_platform_data cryp1_platform_data = {
	.mem_to_engine = {
		.dir = STEDMA40_MEM_TO_PERIPH,
		.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
		.dst_dev_type = DB8500_DMA_DEV48_CAC1_TX,
		.src_info.data_width = STEDMA40_WORD_WIDTH,
		.dst_info.data_width = STEDMA40_WORD_WIDTH,
		.mode = STEDMA40_MODE_LOGICAL,
		.src_info.psize = STEDMA40_PSIZE_LOG_4,
		.dst_info.psize = STEDMA40_PSIZE_LOG_4,
	},
	.engine_to_mem = {
		.dir = STEDMA40_PERIPH_TO_MEM,
		.src_dev_type = DB8500_DMA_DEV48_CAC1_RX,
		.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
		.src_info.data_width = STEDMA40_WORD_WIDTH,
		.dst_info.data_width = STEDMA40_WORD_WIDTH,
		.mode = STEDMA40_MODE_LOGICAL,
		.src_info.psize = STEDMA40_PSIZE_LOG_4,
		.dst_info.psize = STEDMA40_PSIZE_LOG_4,
	}
};

struct platform_device ux500_cryp1_device = {
	.name = "cryp1",
	.id = -1,
	.dev = {
		.platform_data = &cryp1_platform_data
	},
	.num_resources = ARRAY_SIZE(ux500_cryp1_resources),
	.resource = ux500_cryp1_resources
};

void __init amba_add_devices(struct amba_device *devs[], int num)
{
	int i;

	for (i = 0; i < num; i++) {
		struct amba_device *d = devs[i];
		amba_device_register(d, &iomem_resource);
	}
}
