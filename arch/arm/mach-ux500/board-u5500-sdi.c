/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Hanumath Prasad <ulf.hansson@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/amba/mmci.h>
#include <linux/mmc/host.h>
#include <linux/gpio.h>

#include <plat/pincfg.h>
#include <plat/ste_dma40.h>
#include <mach/db5500-regs.h>
#include <mach/ste-dma40-db5500.h>

#include "pins-db5500.h"
#include "devices-db5500.h"
#include "board-u5500.h"

/*
 * SDI0 (EMMC)
 */
#ifdef CONFIG_STE_DMA40
struct stedma40_chan_cfg u5500_sdi0_dma_cfg_rx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_PERIPH_TO_MEM,
	.src_dev_type = DB5500_DMA_DEV24_SDMMC0_RX,
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};

static struct stedma40_chan_cfg u5500_sdi0_dma_cfg_tx = {
	.mode = STEDMA40_MODE_LOGICAL,
	.dir = STEDMA40_MEM_TO_PERIPH,
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
	.dst_dev_type = DB5500_DMA_DEV24_SDMMC0_TX,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};
#endif
/*
 * SDI1 (SD/MMC)
 */
#ifdef CONFIG_STE_DMA40
static struct stedma40_chan_cfg sdi1_dma_cfg_rx = {
	.dir = STEDMA40_PERIPH_TO_MEM,
	.src_dev_type = DB5500_DMA_DEV25_SDMMC1_RX,
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};

static struct stedma40_chan_cfg sdi1_dma_cfg_tx = {
	.dir = STEDMA40_MEM_TO_PERIPH,
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
	.dst_dev_type = DB5500_DMA_DEV25_SDMMC1_TX,
	.src_info.data_width = STEDMA40_WORD_WIDTH,
	.dst_info.data_width = STEDMA40_WORD_WIDTH,
};
#endif

static struct mmci_platform_data u5500_sdi0_data = {
	.ocr_mask	= MMC_VDD_165_195,
	.f_max		= 50000000,
	.capabilities	= MMC_CAP_4_BIT_DATA |
				MMC_CAP_8_BIT_DATA |
				MMC_CAP_MMC_HIGHSPEED,
	.gpio_cd	= -1,
	.gpio_wp	= -1,
#ifdef CONFIG_STE_DMA40
	.dma_filter	= stedma40_filter,
	.dma_rx_param	= &u5500_sdi0_dma_cfg_rx,
	.dma_tx_param	= &u5500_sdi0_dma_cfg_tx,
#endif
};

static u32 u5500_sdi1_vdd_handler(struct device *dev, unsigned int vdd,
		unsigned char power_mode)
{
	/*
	* Level shifter voltage should depend on vdd to when deciding
	* on either 1.8V or 2.9V. Once the decision has been made the
	* level shifter must be disabled and re-enabled with a changed
	* select signal in order to switch the voltage. Since there is
	* no framework support yet for indicating 1.8V in vdd, use the
	* default 2.9V.
	*/
	if (power_mode == MMC_POWER_UP)
		gpio_set_value_cansleep(GPIO_MMC_CARD_CTRL, 1);
	else if (power_mode == MMC_POWER_OFF)
		gpio_set_value_cansleep(GPIO_MMC_CARD_CTRL, 0);
	return 0;
}

static struct mmci_platform_data u5500_sdi1_data = {
	.vdd_handler    = u5500_sdi1_vdd_handler,
	.ocr_mask       = MMC_VDD_29_30,
	.f_max          = 50000000,
	.capabilities	= MMC_CAP_4_BIT_DATA |
				MMC_CAP_MMC_HIGHSPEED,
	.gpio_cd        = GPIO_SDMMC_CD,
	.gpio_wp        = -1,
	.cd_invert	= true,
#ifdef CONFIG_STE_DMA40
	.dma_filter	= stedma40_filter,
	.dma_rx_param	= &sdi1_dma_cfg_rx,
	.dma_tx_param	= &sdi1_dma_cfg_tx,
#endif
};

static void sdi1_configure(void)
{
	int pin[2];
	int ret;

	/* Level-shifter GPIOs */
	pin[0] = GPIO_MMC_CARD_CTRL;
	pin[1] = GPIO_MMC_CARD_VSEL;

	ret = gpio_request(pin[0], "MMC_CARD_CTRL");
	if (!ret)
		ret = gpio_request(pin[1], "MMC_CARD_VSEL");

	if (ret) {
		pr_err("mach-u5500: error in configuring \
			GPIO pins for MMC\n");
		return;
	}
	 /* Select the default 2.9V and eanble level shifter */
	gpio_direction_output(pin[0], 1);
	gpio_direction_output(pin[1], 1);

}

void __init u5500_sdi_init(void)
{
	db5500_add_sdi0(&u5500_sdi0_data);
	sdi1_configure();
	db5500_add_sdi1(&u5500_sdi1_data);
}

