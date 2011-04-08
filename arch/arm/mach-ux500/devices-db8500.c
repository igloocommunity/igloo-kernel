/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 *
 * Author: Pierre Peiffer <pierre.peiffer@stericsson.com> for ST-Ericsson.
 * for the System Trace Module part.
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl022.h>
#include <plat/pincfg.h>

#include <plat/ste_dma40.h>

#include <mach/devices.h>
#include <mach/hardware.h>
#include <mach/setup.h>
#include <video/mcde.h>
#include <mach/prcmu-fw-api.h>
#include <mach/prcmu-regs.h>
#include <mach/ste-dma40-db8500.h>
#include <trace/stm.h>

#include "pins-db8500.h"

static struct resource dma40_resources[] = {
	[0] = {
		.start = U8500_DMA_BASE,
		.end   = U8500_DMA_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
		.name  = "base",
	},
	[1] = {
		.start = U8500_DMA_LCPA_BASE,
		.end   = U8500_DMA_LCPA_BASE + 2 * SZ_1K - 1,
		.flags = IORESOURCE_MEM,
		.name  = "lcpa",
	},
	[2] = {
		.start = IRQ_DB8500_DMA,
		.end   = IRQ_DB8500_DMA,
		.flags = IORESOURCE_IRQ,
	}
};

/* Default configuration for physcial memcpy */
struct stedma40_chan_cfg dma40_memcpy_conf_phy = {
	.mode = STEDMA40_MODE_PHYSICAL,
	.dir = STEDMA40_MEM_TO_MEM,

	.src_info.data_width = STEDMA40_BYTE_WIDTH,
	.src_info.psize = STEDMA40_PSIZE_PHY_1,
	.src_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,

	.dst_info.data_width = STEDMA40_BYTE_WIDTH,
	.dst_info.psize = STEDMA40_PSIZE_PHY_1,
	.dst_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,
};
/* Default configuration for logical memcpy */
struct stedma40_chan_cfg dma40_memcpy_conf_log = {
	.dir = STEDMA40_MEM_TO_MEM,

	.src_info.data_width = STEDMA40_BYTE_WIDTH,
	.src_info.psize = STEDMA40_PSIZE_LOG_1,
	.src_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,

	.dst_info.data_width = STEDMA40_BYTE_WIDTH,
	.dst_info.psize = STEDMA40_PSIZE_LOG_1,
	.dst_info.flow_ctrl = STEDMA40_NO_FLOW_CTRL,
};

/*
 * Mapping between destination event lines and physical device address.
 * The event line is tied to a device and therefore the address is constant.
 * When the address comes from a primecell it will be configured in runtime
 * and we set the address to -1 as a placeholder.
 */
static const dma_addr_t dma40_tx_map[DB8500_DMA_NR_DEV] = {
	/* MUSB - these will be runtime-reconfigured */
	[DB8500_DMA_DEV39_USB_OTG_OEP_8] = -1,
	[DB8500_DMA_DEV16_USB_OTG_OEP_7_15] = -1,
	[DB8500_DMA_DEV17_USB_OTG_OEP_6_14] = -1,
	[DB8500_DMA_DEV18_USB_OTG_OEP_5_13] = -1,
	[DB8500_DMA_DEV19_USB_OTG_OEP_4_12] = -1,
	[DB8500_DMA_DEV36_USB_OTG_OEP_3_11] = -1,
	[DB8500_DMA_DEV37_USB_OTG_OEP_2_10] = -1,
	[DB8500_DMA_DEV38_USB_OTG_OEP_1_9] = -1,
	/* PrimeCells - run-time configured */
	[DB8500_DMA_DEV0_SPI0_TX] = -1,
	[DB8500_DMA_DEV1_SD_MMC0_TX] = -1,
	[DB8500_DMA_DEV2_SD_MMC1_TX] = -1,
	[DB8500_DMA_DEV3_SD_MMC2_TX] = -1,
	[DB8500_DMA_DEV8_SSP0_TX] = -1,
	[DB8500_DMA_DEV9_SSP1_TX] = -1,
	[DB8500_DMA_DEV11_UART2_TX] = -1,
	[DB8500_DMA_DEV12_UART1_TX] = -1,
	[DB8500_DMA_DEV13_UART0_TX] = -1,
	[DB8500_DMA_DEV28_SD_MM2_TX] = -1,
	[DB8500_DMA_DEV29_SD_MM0_TX] = -1,
	[DB8500_DMA_DEV32_SD_MM1_TX] = -1,
	[DB8500_DMA_DEV33_SPI2_TX] = -1,
	[DB8500_DMA_DEV35_SPI1_TX] = -1,
	[DB8500_DMA_DEV40_SPI3_TX] = -1,
	[DB8500_DMA_DEV41_SD_MM3_TX] = -1,
	[DB8500_DMA_DEV42_SD_MM4_TX] = -1,
	[DB8500_DMA_DEV43_SD_MM5_TX] = -1,
};

/* Mapping between source event lines and physical device address */
static const dma_addr_t dma40_rx_map[DB8500_DMA_NR_DEV] = {
	/* MUSB - these will be runtime-reconfigured */
	[DB8500_DMA_DEV39_USB_OTG_IEP_8] = -1,
	[DB8500_DMA_DEV16_USB_OTG_IEP_7_15] = -1,
	[DB8500_DMA_DEV17_USB_OTG_IEP_6_14] = -1,
	[DB8500_DMA_DEV18_USB_OTG_IEP_5_13] = -1,
	[DB8500_DMA_DEV19_USB_OTG_IEP_4_12] = -1,
	[DB8500_DMA_DEV36_USB_OTG_IEP_3_11] = -1,
	[DB8500_DMA_DEV37_USB_OTG_IEP_2_10] = -1,
	[DB8500_DMA_DEV38_USB_OTG_IEP_1_9] = -1,
	/* PrimeCells */
	[DB8500_DMA_DEV0_SPI0_RX] = -1,
	[DB8500_DMA_DEV1_SD_MMC0_RX] = -1,
	[DB8500_DMA_DEV2_SD_MMC1_RX] = -1,
	[DB8500_DMA_DEV3_SD_MMC2_RX] = -1,
	[DB8500_DMA_DEV8_SSP0_RX] = -1,
	[DB8500_DMA_DEV9_SSP1_RX] = -1,
	[DB8500_DMA_DEV11_UART2_RX] = -1,
	[DB8500_DMA_DEV12_UART1_RX] = -1,
	[DB8500_DMA_DEV13_UART0_RX] = -1,
	[DB8500_DMA_DEV28_SD_MM2_RX] = -1,
	[DB8500_DMA_DEV29_SD_MM0_RX] = -1,
	[DB8500_DMA_DEV32_SD_MM1_RX] = -1,
	[DB8500_DMA_DEV33_SPI2_RX] = -1,
	[DB8500_DMA_DEV35_SPI1_RX] = -1,
	[DB8500_DMA_DEV40_SPI3_RX] = -1,
	[DB8500_DMA_DEV41_SD_MM3_RX] = -1,
	[DB8500_DMA_DEV42_SD_MM4_RX] = -1,
	[DB8500_DMA_DEV43_SD_MM5_RX] = -1,
};

/* Reserved event lines for memcpy only */
static int dma40_memcpy_event[] = {
	DB8500_DMA_MEMCPY_TX_0,
	DB8500_DMA_MEMCPY_TX_1,
	DB8500_DMA_MEMCPY_TX_2,
	DB8500_DMA_MEMCPY_TX_3,
	DB8500_DMA_MEMCPY_TX_4,
	DB8500_DMA_MEMCPY_TX_5,
};

static struct stedma40_platform_data dma40_plat_data = {
	.dev_len = DB8500_DMA_NR_DEV,
	.dev_rx = dma40_rx_map,
	.dev_tx = dma40_tx_map,
	.memcpy = dma40_memcpy_event,
	.memcpy_len = ARRAY_SIZE(dma40_memcpy_event),
	.memcpy_conf_phy = &dma40_memcpy_conf_phy,
	.memcpy_conf_log = &dma40_memcpy_conf_log,
	.disabled_channels = {-1},
};

struct platform_device u8500_dma40_device = {
	.dev = {
		.platform_data = &dma40_plat_data,
	},
	.name = "dma40",
	.id = 0,
	.num_resources = ARRAY_SIZE(dma40_resources),
	.resource = dma40_resources
};

static struct resource u8500_shrm_resources[] = {
	[0] = {
		.start = U8500_SHRM_GOP_INTERRUPT_BASE,
		.end = U8500_SHRM_GOP_INTERRUPT_BASE + ((4*4)-1),
		.name = "shrm_gop_register_base",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_CA_WAKE_REQ_V1,
		.end = IRQ_CA_WAKE_REQ_V1,
		.name = "ca_irq_wake_req",
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_AC_READ_NOTIFICATION_0_V1,
		.end = IRQ_AC_READ_NOTIFICATION_0_V1,
		.name = "ac_read_notification_0_irq",
		.flags = IORESOURCE_IRQ,
	},
	[3] = {
		.start = IRQ_AC_READ_NOTIFICATION_1_V1,
		.end = IRQ_AC_READ_NOTIFICATION_1_V1,
		.name = "ac_read_notification_1_irq",
		.flags = IORESOURCE_IRQ,
	},
	[4] = {
		.start = IRQ_CA_MSG_PEND_NOTIFICATION_0_V1,
		.end = IRQ_CA_MSG_PEND_NOTIFICATION_0_V1,
		.name = "ca_msg_pending_notification_0_irq",
		.flags = IORESOURCE_IRQ,
	},
	[5] = {
		.start = IRQ_CA_MSG_PEND_NOTIFICATION_1_V1,
		.end = IRQ_CA_MSG_PEND_NOTIFICATION_1_V1,
		.name = "ca_msg_pending_notification_1_irq",
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device u8500_shrm_device = {
	.name = "u8500_shrm",
	.id = 0,
	.dev = {
		.init_name = "shrm_bus",
		.coherent_dma_mask = ~0,
	},

	.num_resources = ARRAY_SIZE(u8500_shrm_resources),
	.resource = u8500_shrm_resources
};

static struct resource mcde_resources[] = {
	[0] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_MCDE_BASE,
		.end   = U8500_MCDE_BASE + U8500_MCDE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK1_BASE,
		.end   = U8500_DSI_LINK1_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK2_BASE,
		.end   = U8500_DSI_LINK2_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.name  = MCDE_IO_AREA,
		.start = U8500_DSI_LINK3_BASE,
		.end   = U8500_DSI_LINK3_BASE + U8500_DSI_LINK_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[4] = {
		.name  = MCDE_IRQ,
		.start = IRQ_DB8500_DISP,
		.end   = IRQ_DB8500_DISP,
		.flags = IORESOURCE_IRQ,
	},
};

static int mcde_platform_enable(void)
{
	return prcmu_enable_dsipll();
}

static int mcde_platform_disable(void)
{
	return prcmu_disable_dsipll();
}

static struct mcde_platform_data mcde_pdata = {
	.num_dsilinks = 3,
	/*
	 * [0] = 3: 24 bits DPI: connect LSB Ch B to D[0:7]
	 * [3] = 4: 24 bits DPI: connect MID Ch B to D[24:31]
	 * [4] = 5: 24 bits DPI: connect MSB Ch B to D[32:39]
	 *
	 * [1] = 3: TV out     : connect LSB Ch B to D[8:15]
	 */
#define DONT_CARE 0
	.outmux = { 3, 3, DONT_CARE, 4, 5 },
#undef DONT_CARE
	.syncmux = 0x00,  /* DPI channel A and B on output pins A and B resp */
	.num_channels = 4,
	.num_overlays = 6,
	.regulator_vana_id = "v-ana",
	.regulator_mcde_epod_id = "vsupply",
	.regulator_esram_epod_id = "v-esram34",
	.clock_dsi_id = "hdmi",
	.clock_dsi_lp_id = "tv",
	.clock_dpi_id = "lcd",
	.clock_mcde_id = "mcde",
	.platform_enable = mcde_platform_enable,
	.platform_disable = mcde_platform_disable,
};

struct platform_device u8500_mcde_device = {
	.name = "mcde",
	.id = -1,
	.dev = {
		.platform_data = &mcde_pdata,
	},
	.num_resources = ARRAY_SIZE(mcde_resources),
	.resource = mcde_resources,
};

static struct resource b2r2_resources[] = {
	[0] = {
		.start	= U8500_B2R2_BASE,
		.end	= U8500_B2R2_BASE + ((4*1024)-1),
		.name	= "b2r2_base",
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name  = "B2R2_IRQ",
		.start = IRQ_DB8500_B2R2,
		.end   = IRQ_DB8500_B2R2,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device u8500_b2r2_device = {
	.name	= "b2r2",
	.id	= 0,
	.dev	= {
		.init_name = "b2r2_bus",
		.coherent_dma_mask = ~0,
	},
	.num_resources	= ARRAY_SIZE(b2r2_resources),
	.resource	= b2r2_resources,
};

/*
 * WATCHDOG
 */

static struct resource ux500_wdt_resources[] = {
	[0] = {
		.start  = U8500_TWD_BASE,
		.end    = U8500_TWD_BASE+0x37,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_LOCALWDOG,
		.end  = IRQ_LOCALWDOG,
		.flags  = IORESOURCE_IRQ,
	}
};

struct platform_device ux500_wdt_device = {
	.name           = "mpcore_wdt",
	.id             = -1,
	.resource       = ux500_wdt_resources,
	.num_resources  = ARRAY_SIZE(ux500_wdt_resources),
};

/*
 * Thermal Sensor
 */

static struct resource u8500_thsens_resources[] = {
	{
		.name = "IRQ_HOTMON_LOW",
		.start  = IRQ_PRCMU_HOTMON_LOW,
		.end    = IRQ_PRCMU_HOTMON_LOW,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name = "IRQ_HOTMON_HIGH",
		.start  = IRQ_PRCMU_HOTMON_HIGH,
		.end    = IRQ_PRCMU_HOTMON_HIGH,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device u8500_thsens_device = {
	.name           = "db8500_temp",
	.resource       = u8500_thsens_resources,
	.num_resources  = ARRAY_SIZE(u8500_thsens_resources),
};

void dma40_u8500ed_fixup(void)
{
	dma40_plat_data.memcpy = NULL;
	dma40_plat_data.memcpy_len = 0;
	dma40_resources[0].start = U8500_DMA_BASE_ED;
	dma40_resources[0].end = U8500_DMA_BASE_ED + SZ_4K - 1;
	dma40_resources[1].start = U8500_DMA_LCPA_BASE_ED;
	dma40_resources[1].end = U8500_DMA_LCPA_BASE_ED + 2 * SZ_1K - 1;
}

struct resource keypad_resources[] = {
	[0] = {
		.start = U8500_SKE_BASE,
		.end = U8500_SKE_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_DB8500_KB,
		.end = IRQ_DB8500_KB,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device u8500_ske_keypad_device = {
	.name = "nmk-ske-keypad",
	.id = -1,
	.num_resources = ARRAY_SIZE(keypad_resources),
	.resource = keypad_resources,
};

#ifdef CONFIG_STM_TRACE
static pin_cfg_t mop500_stm_mipi34_pins[] = {
	GPIO70_STMAPE_CLK,
	GPIO71_STMAPE_DAT3,
	GPIO72_STMAPE_DAT2,
	GPIO73_STMAPE_DAT1,
	GPIO74_STMAPE_DAT0,
	GPIO75_U2_RXD,
	GPIO76_U2_TXD,
};

static pin_cfg_t mop500_stm_mipi60_pins[] = {
	GPIO153_U2_RXD,
	GPIO154_U2_TXD,
	GPIO155_STMAPE_CLK,
	GPIO156_STMAPE_DAT3,
	GPIO157_STMAPE_DAT2,
	GPIO158_STMAPE_DAT1,
	GPIO159_STMAPE_DAT0,
};

static pin_cfg_t mop500_ske_pins[] = {
	GPIO153_KP_I7,
	GPIO154_KP_I6,
	GPIO155_KP_I5,
	GPIO156_KP_I4,
	GPIO157_KP_O7,
	GPIO158_KP_O6,
	GPIO159_KP_O5,
};

static int stm_ste_disable_ape_on_mipi60(void)
{
	int retval;

	retval = nmk_config_pins_sleep(ARRAY_AND_SIZE(mop500_stm_mipi60_pins));
	if (retval)
		pr_err("STM: Failed to disable MIPI60\n");
	else {
		retval = nmk_config_pins(ARRAY_AND_SIZE(mop500_ske_pins));
		if (retval)
			pr_err("STM: Failed to enable SKE gpio\n");
	}
	return retval;
}

/*
 * Manage STM output pins connection (MIP34/MIPI60 connectors)
 */
static int stm_ste_connection(enum stm_connection_type con_type)
{
	int retval = -EINVAL;
	u32 gpiocr = readl(PRCM_GPIOCR);

	if (con_type != STM_DISCONNECT) {
		/*  Always enable MIPI34 GPIO pins */
		retval = nmk_config_pins(
				ARRAY_AND_SIZE(mop500_stm_mipi34_pins));
		if (retval) {
			pr_err("STM: Failed to enable MIPI34\n");
			return retval;
		}
	}

	switch (con_type) {
	case STM_DEFAULT_CONNECTION:
	case STM_STE_MODEM_ON_MIPI34_NONE_ON_MIPI60:
		/* Enable altC3 on GPIO70-74 (STMMOD) & GPIO75-76 (UARTMOD) */
		gpiocr |= (PRCM_GPIOCR_DBG_STM_MOD_CMD1
				| PRCM_GPIOCR_DBG_UARTMOD_CMD0);
		writel(gpiocr, PRCM_GPIOCR);
		retval = stm_ste_disable_ape_on_mipi60();
		break;

	case STM_STE_APE_ON_MIPI34_NONE_ON_MIPI60:
		/* Disable altC3 on GPIO70-74 (STMMOD) & GPIO75-76 (UARTMOD) */
		gpiocr &= ~(PRCM_GPIOCR_DBG_STM_MOD_CMD1
				| PRCM_GPIOCR_DBG_UARTMOD_CMD0);
		writel(gpiocr, PRCM_GPIOCR);
		retval = stm_ste_disable_ape_on_mipi60();
		break;

	case STM_STE_MODEM_ON_MIPI34_APE_ON_MIPI60:
		/* Enable altC3 on GPIO70-74 (STMMOD) and GPIO75-76 (UARTMOD) */
		gpiocr |= (PRCM_GPIOCR_DBG_STM_MOD_CMD1
				| PRCM_GPIOCR_DBG_UARTMOD_CMD0);
		writel(gpiocr, PRCM_GPIOCR);

		/* Enable APE on MIPI60 */
		retval = nmk_config_pins_sleep(ARRAY_AND_SIZE(mop500_ske_pins));
		if (retval)
			pr_err("STM: Failed to disable SKE GPIO\n");
		else {
			retval = nmk_config_pins(
					ARRAY_AND_SIZE(mop500_stm_mipi60_pins));
			if (retval)
				pr_err("STM: Failed to enable MIPI60\n");
		}
		break;

	case STM_DISCONNECT:
		retval = nmk_config_pins_sleep(
				ARRAY_AND_SIZE(mop500_stm_mipi34_pins));
		if (retval)
			pr_err("STM: Failed to disable MIPI34\n");

		retval = stm_ste_disable_ape_on_mipi60();
		break;

	default:
		pr_err("STM: bad connection type\n");
		break;
	}
	return retval;
}

/* Possible STM sources (masters) on ux500 */
enum stm_master {
	STM_ARM0 =	0,
	STM_ARM1 =	1,
	STM_SVA =	2,
	STM_SIA =	3,
	STM_SIA_XP70 =	4,
	STM_PRCMU =	5,
	STM_MCSBAG =	9
};

#define STM_ENABLE_ARM0		BIT(STM_ARM0)
#define STM_ENABLE_ARM1		BIT(STM_ARM1)
#define STM_ENABLE_SVA		BIT(STM_SVA)
#define STM_ENABLE_SIA		BIT(STM_SIA)
#define STM_ENABLE_SIA_XP70	BIT(STM_SIA_XP70)
#define STM_ENABLE_PRCMU	BIT(STM_PRCMU)
#define STM_ENABLE_MCSBAG	BIT(STM_MCSBAG)

/*
 * These are the channels used by NMF and some external softwares
 * expect the NMF traces to be output on these channels
 * For legacy reason, we need to reserve them.
 */
static const s16 stm_channels_reserved[] = {
	100,	/* NMF MPCEE channel */
	101,	/* NMF CM channel */
	151,	/* NMF HOSTEE channel */
};

/* On Ux500 we 2 consecutive STMs therefore 512 channels available */
static struct stm_platform_data stm_pdata = {
	.regs_phys_base       = U8500_STM_REG_BASE,
	.channels_phys_base   = U8500_STM_BASE,
	.id_mask              = 0x000fffff,   /* Ignore revisions differences */
	.channels_reserved    = stm_channels_reserved,
	.channels_reserved_sz = ARRAY_SIZE(stm_channels_reserved),
	/* Enable all except MCSBAG */
	.masters_enabled      = STM_ENABLE_ARM0 | STM_ENABLE_ARM1 |
				STM_ENABLE_SVA | STM_ENABLE_PRCMU |
				STM_ENABLE_SIA | STM_ENABLE_SIA_XP70,
	/* Provide function for MIPI34/MIPI60 STM connection */
	.stm_connection       = stm_ste_connection,
};

struct platform_device ux500_stm_device = {
	.name = "stm",
	.id = -1,
	.dev = {
		.platform_data = &stm_pdata,
	},
};
#endif /* CONFIG_UX500_STM */
