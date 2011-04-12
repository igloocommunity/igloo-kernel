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

#ifdef CONFIG_STE_TRACE_MODEM
#include <linux/db8500-modem-trace.h>
#endif

#ifdef CONFIG_STE_TRACE_MODEM
static struct resource trace_resource = {
	.start	= 0,
	.end	= 0,
	.name	= "db8500-trace-area",
	.flags	= IORESOURCE_MEM
};

static struct db8500_trace_platform_data trace_pdata = {
	.ape_base = U8500_APE_BASE,
	.modem_base = U8500_MODEM_BASE,
};

struct platform_device u8500_trace_modem = {
	.name	= "db8500-modem-trace",
	.id = 0,
	.dev = {
		.init_name = "db8500-modem-trace",
		.platform_data = &trace_pdata,
	},
	.num_resources = 1,
	.resource = &trace_resource,
};

static int __init early_trace_modem(char *p)
{
	struct resource *data = &trace_resource;
	u32 size = memparse(p, &p);

	if (*p == '@')
		data->start = memparse(p + 1, &p);
	data->end = data->start + size;

	return 0;
}

early_param("mem_mtrace", early_trace_modem);
#endif

#ifdef CONFIG_HWMEM
struct platform_device ux500_hwmem_device = {
	.name = "hwmem",
};
#endif

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
