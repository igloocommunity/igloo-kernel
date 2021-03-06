/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/amba/pl022.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2s/i2s.h>
#include <linux/mfd/abx500.h>
#include <linux/led-lm3530.h>
#include <../drivers/staging/ste_rmi4/synaptics_i2c_rmi4.h>
#include <linux/input/matrix_keypad.h>
#include <linux/lsm303dlh.h>
#include <linux/leds-ab5500.h>
#include <linux/cyttsp.h>

#include <video/av8100.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/pincfg.h>
#include <plat/i2c.h>

#include <mach/hardware.h>
#include <mach/ste-dma40-db5500.h>
#include <mach/msp.h>
#include <mach/devices.h>
#include <mach/setup.h>
#include <mach/db5500-keypad.h>
#include <mach/crypto-ux500.h>
#include <mach/usb.h>
#include <mach/abx500-accdet.h>

#include "pins-db5500.h"
#include "pins.h"
#include "devices-db5500.h"
#include "board-u5500.h"
#include "board-u5500-bm.h"
#include "board-u5500-wlan.h"
#include "board-ux500-usb.h"

/*
 * LSM303DLH
 */

static struct lsm303dlh_platform_data __initdata lsm303dlh_pdata = {
	.name_a = "lsm303dlh.0",
	.name_m = "lsm303dlh.1",
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negative_x = 0,
	.negative_y = 0,
	.negative_z = 1,
};

/*
 * Touchscreen
 */
static struct synaptics_rmi4_platform_data rmi4_i2c_platformdata = {
	.irq_number	= NOMADIK_GPIO_TO_IRQ(179),
	.irq_type	= (IRQF_TRIGGER_FALLING | IRQF_SHARED),
#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE) &&      \
			CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE == 270
	.x_flip		= true,
	.y_flip		= false,
#else
	.x_flip		= false,
	.y_flip		= true,
#endif
	.regulator_en	= true,
};

static struct av8100_platform_data av8100_plat_data = {
	.irq = NOMADIK_GPIO_TO_IRQ(223),
	.reset = 225,
	.alt_powerupseq = true,
	.mclk_freq = 1, /* MCLK_RNG_22_27 */
};

/*
 * leds LM3530
 */
static struct lm3530_platform_data u5500_als_platform_data = {
	.mode = LM3530_BL_MODE_MANUAL,
	.als_input_mode = LM3530_INPUT_ALS1,
	.max_current = LM3530_FS_CURR_26mA,
	.pwm_pol_hi = true,
	.als_avrg_time = LM3530_ALS_AVRG_TIME_512ms,
	.brt_ramp_law = 1,	/* Linear */
	.brt_ramp_fall = LM3530_RAMP_TIME_8s,
	.brt_ramp_rise = LM3530_RAMP_TIME_8s,
	.als1_resistor_sel = LM3530_ALS_IMPD_13_53kOhm,
	.als2_resistor_sel = LM3530_ALS_IMPD_Z,
	.als_vmin = 730,	/* mV */
	.als_vmax = 1020,	/* mV */
	.brt_val = 0x7F,	/* Max brightness */
};


/* leds-ab5500 */
static struct ab5500_hvleds_platform_data ab5500_hvleds_data = {
	.hw_fade = false,
	.leds = {
		[0] = {
			.name = "red",
			.led_on = true,
			.led_id = 0,
			.fade_hi = 255,
			.fade_lo = 0,
			.max_current = 10, /* wrong value may damage h/w */
		},
		[1] = {
			.name = "green",
			.led_on = true,
			.led_id = 1,
			.fade_hi = 255,
			.fade_lo = 0,
			.max_current = 10, /* wrong value may damage h/w */
		},
		[2] {
			.name = "blue",
			.led_on = true,
			.led_id = 2,
			.fade_hi = 255,
			.fade_lo = 0,
			.max_current = 10, /* wrong value may damage h/w */
		},
	},
};

static struct ab5500_ponkey_platform_data ab5500_ponkey_data = {
	/*
	 * Shutdown time in secs. Can be set
	 * to 10sec, 5sec and 0sec(disabled)
	 */
	.shutdown_secs = 10,
};

/*
 * I2C
 */

#define U5500_I2C_CONTROLLER(id, _slsu, _tft, _rft, clk, t_out, _sm) \
static struct nmk_i2c_controller u5500_i2c##id##_data = { \
	/*				\
	 * slave data setup time, which is	\
	 * 250 ns,100ns,10ns which is 14,6,2	\
	 * respectively for a 48 Mhz	\
	 * i2c clock			\
	 */				\
	.slsu		= _slsu,	\
	/* Tx FIFO threshold */		\
	.tft		= _tft,		\
	/* Rx FIFO threshold */		\
	.rft		= _rft,		\
	/* std. mode operation */	\
	.clk_freq	= clk,		\
	/* Slave response timeout(ms) */\
	.timeout	= t_out,	\
	.sm		= _sm,		\
}

/*
 * The board uses 3 i2c controllers, initialize all of
 * them with slave data setup time of 250 ns,
 * Tx & Rx FIFO threshold values as 1 and standard
 * mode of operation
 */

U5500_I2C_CONTROLLER(1,	0xe, 1, 10, 400000, 200, I2C_FREQ_MODE_FAST);
U5500_I2C_CONTROLLER(2,	0xe, 1, 10, 400000, 200, I2C_FREQ_MODE_FAST);
U5500_I2C_CONTROLLER(3,	0xe, 1, 10, 400000, 200, I2C_FREQ_MODE_FAST);

static struct i2c_board_info __initdata u5500_i2c1_devices[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x4B),
		.platform_data = &rmi4_i2c_platformdata,
	},
};

static struct i2c_board_info __initdata u5500v1_i2c2_sensor_devices[] = {
 	{
 		/* LSM303DLH Accelerometer */
 		I2C_BOARD_INFO("lsm303dlh_a", 0x19),
 		.platform_data = &lsm303dlh_pdata,
 	},
 	{
		/* LSM303DLH Magnetometer */
		I2C_BOARD_INFO("lsm303dlh_m", 0x1E),
		.platform_data = &lsm303dlh_pdata,
	},
};

static struct i2c_board_info __initdata u5500v2_i2c2_sensor_devices[] = {
	{
		/* LSM303DLHC Accelerometer */
		I2C_BOARD_INFO("lsm303dlhc_a", 0x19),
		.platform_data = &lsm303dlh_pdata,
	},
	{
		/* LSM303DLH Magnetometer */
		I2C_BOARD_INFO("lsm303dlh_m", 0x1E),
		.platform_data = &lsm303dlh_pdata,
	},
};

static struct i2c_board_info __initdata u5500_i2c2_devices[] = {
	{
		/* Backlight */
		I2C_BOARD_INFO("lm3530-led", 0x36),
		.platform_data = &u5500_als_platform_data,
	},
	{
		I2C_BOARD_INFO("av8100", 0x70),
		.platform_data = &av8100_plat_data,
	},
};

/*
 * Keypad
 */

#define ROW_PIN_I0      128
#define ROW_PIN_I1      130
#define ROW_PIN_I2      132
#define ROW_PIN_I3      134
#define COL_PIN_O4      137
#define COL_PIN_O5      139

static int db5500_kp_rows[] = {
	ROW_PIN_I0, ROW_PIN_I1, ROW_PIN_I2, ROW_PIN_I3,
};

static int db5500_kp_cols[] = {
	COL_PIN_O4, COL_PIN_O5,
};

static bool db5500_config;
static int db5500_set_gpio_row(int gpio)
{
	int ret = -1;


	if (!db5500_config) {
		ret = gpio_request(gpio, "db5500_kpd");
		if (ret < 0) {
			pr_err("db5500_set_gpio_row: gpio request failed\n");
			return ret;
		}
	}

	ret = gpio_direction_output(gpio, 1);
	if (ret < 0) {
		pr_err("db5500_set_gpio_row: gpio direction failed\n");
		gpio_free(gpio);
	}

	return ret;
}

static int db5500_kp_init(void)
{
	struct ux500_pins *pins;
	int ret, i;

	pins = ux500_pins_get("db5500_kp");
	if (pins)
		ux500_pins_enable(pins);

	for (i = 0; i < ARRAY_SIZE(db5500_kp_rows); i++) {
		ret = db5500_set_gpio_row(db5500_kp_rows[i]);
		if (ret < 0) {
			pr_err("db5500_kp_init: failed init\n");
			return ret;
		}
	}

	if (!db5500_config)
		db5500_config = true;

	return 0;
}

static int db5500_kp_exit(void)
{
	struct ux500_pins *pins;

	pins = ux500_pins_get("db5500_kp");
	if (pins)
		ux500_pins_disable(pins);

	return 0;
}

static const unsigned int u5500_keymap[] = {
	KEY(4, 0, KEY_CAMERA), /* Camera2 */
	KEY(4, 1, KEY_CAMERA_FOCUS), /* Camera1 */
	KEY(4, 2, KEY_MENU),
	KEY(4, 3, KEY_BACK),
	KEY(5, 2, KEY_SEND),
	KEY(5, 3, KEY_HOME),
#ifndef CONFIG_INPUT_AB8500_PONKEY
	/* AB5500 ONSWa is also hooked up to this key */
	KEY(8, 0, KEY_END),
#endif
	KEY(8, 1, KEY_VOLUMEUP),
	KEY(8, 2, KEY_VOLUMEDOWN),
};

static struct matrix_keymap_data u5500_keymap_data = {
	.keymap		= u5500_keymap,
	.keymap_size	= ARRAY_SIZE(u5500_keymap),
};

static struct db5500_keypad_platform_data u5500_keypad_board = {
	.init           = db5500_kp_init,
	.exit           = db5500_kp_exit,
	.gpio_input_pins = db5500_kp_rows,
	.gpio_output_pins = db5500_kp_cols,
	.keymap_data	= &u5500_keymap_data,
	.no_autorepeat	= true,
	.krow		= ARRAY_SIZE(db5500_kp_rows),
	.kcol		= ARRAY_SIZE(db5500_kp_cols),
	.debounce_ms	= 40, /* milliseconds */
	.switch_delay	= 200, /* in jiffies */
};

/*
 * MSP
 */

#define MSP_DMA(num, eventline)					\
static struct stedma40_chan_cfg msp##num##_dma_rx = {		\
	.high_priority = true,					\
	.dir = STEDMA40_PERIPH_TO_MEM,				\
	.src_dev_type = eventline##_RX,				\
	.dst_dev_type = STEDMA40_DEV_DST_MEMORY,		\
	.src_info.psize = STEDMA40_PSIZE_LOG_4,			\
	.dst_info.psize = STEDMA40_PSIZE_LOG_4,			\
};								\
								\
static struct stedma40_chan_cfg msp##num##_dma_tx = {		\
	.high_priority = true,					\
	.dir = STEDMA40_MEM_TO_PERIPH,				\
	.src_dev_type = STEDMA40_DEV_SRC_MEMORY,		\
	.dst_dev_type = eventline##_TX,				\
	.src_info.psize = STEDMA40_PSIZE_LOG_4,			\
	.dst_info.psize = STEDMA40_PSIZE_LOG_4,			\
}

MSP_DMA(0, DB5500_DMA_DEV9_MSP0);
MSP_DMA(1, DB5500_DMA_DEV10_MSP1);
MSP_DMA(2, DB5500_DMA_DEV11_MSP2);

static struct msp_i2s_platform_data u5500_msp0_data = {
	.id		= MSP_0_I2S_CONTROLLER,
	.msp_i2s_dma_rx	= &msp0_dma_rx,
	.msp_i2s_dma_tx	= &msp0_dma_tx,
};

static struct msp_i2s_platform_data u5500_msp1_data = {
	.id		= MSP_1_I2S_CONTROLLER,
	.msp_i2s_dma_rx	= &msp1_dma_rx,
	.msp_i2s_dma_tx	= &msp1_dma_tx,
};

static struct msp_i2s_platform_data u5500_msp2_data = {
	.id		= MSP_2_I2S_CONTROLLER,
	.msp_i2s_dma_rx	= &msp2_dma_rx,
	.msp_i2s_dma_tx	= &msp2_dma_tx,
};

static struct i2s_board_info stm_i2s_board_info[] __initdata = {
	{
		.modalias	= "i2s_device.0",
		.id		= 0,
		.chip_select	= 0,
	},
	{
		.modalias	= "i2s_device.1",
		.id		= 1,
		.chip_select	= 1,
	},
	{
		.modalias	= "i2s_device.2",
		.id		= 2,
		.chip_select	= 2,
	},
};

static void __init u5500_msp_init(void)
{
	db5500_add_msp0_i2s(&u5500_msp0_data);
	db5500_add_msp1_i2s(&u5500_msp1_data);
	db5500_add_msp2_i2s(&u5500_msp2_data);

	i2s_register_board_info(ARRAY_AND_SIZE(stm_i2s_board_info));
}

/*
 * SPI
 */

static struct pl022_ssp_controller u5500_spi3_data = {
	.bus_id		= 1,
	.num_chipselect	= 4,	/* 3 possible CS lines + 1 for tests */
};

static void __init u5500_spi_init(void)
{
	db5500_add_spi3(&u5500_spi3_data);
}

static struct resource ab5500_resources[] = {
	[0] = {
		.start = IRQ_DB5500_PRCMU_ABB,
		.end = IRQ_DB5500_PRCMU_ABB,
		.flags = IORESOURCE_IRQ
	}
};


#ifdef CONFIG_INPUT_AB5500_ACCDET
static struct abx500_accdet_platform_data ab5500_accdet_pdata = {
	       .btn_keycode = KEY_MEDIA,
	       .accdet1_dbth = ACCDET1_TH_300mV | ACCDET1_DB_10ms,
	       .accdet2122_th = ACCDET21_TH_300mV | ACCDET22_TH_300mV,
	       .is_detection_inverted = false,
	};
#endif

static struct ab5500_platform_data ab5500_plf_data = {
	.irq = {
		.base = IRQ_AB5500_BASE,
		.count = AB5500_NR_IRQS,
	},
	.pm_power_off	= true,
	.regulator	= &u5500_ab5500_regulator_data,
#ifdef CONFIG_INPUT_AB5500_ACCDET
	.dev_data[AB5500_DEVID_ACCDET] = &ab5500_accdet_pdata,
	.dev_data_sz[AB5500_DEVID_ACCDET] = sizeof(ab5500_accdet_pdata),
#endif
	.dev_data[AB5500_DEVID_LEDS] = &ab5500_hvleds_data,
	.dev_data_sz[AB5500_DEVID_LEDS] = sizeof(ab5500_hvleds_data),
	.init_settings = (struct abx500_init_settings[]){
			{
				.bank = 0x3,
				.reg = 0x17,
				.setting = 0x0F,
			},
			{
				.bank = 0x3,
				.reg = 0x18,
				.setting = 0x10,
			},
	},
	.init_settings_sz = 2,
#if defined(CONFIG_AB5500_BM)
	.dev_data[AB5500_DEVID_CHARGALG] = &abx500_bm_pt_data,
	.dev_data_sz[AB5500_DEVID_CHARGALG] = sizeof(abx500_bm_pt_data),
	.dev_data[AB5500_DEVID_CHARGER] = &abx500_bm_pt_data,
	.dev_data_sz[AB5500_DEVID_CHARGER] = sizeof(abx500_bm_pt_data),
	.dev_data[AB5500_DEVID_FG] = &abx500_bm_pt_data,
	.dev_data_sz[AB5500_DEVID_FG] = sizeof(abx500_bm_pt_data),
	.dev_data[AB5500_DEVID_BTEMP] = &abx500_bm_pt_data,
	.dev_data_sz[AB5500_DEVID_BTEMP] = sizeof(abx500_bm_pt_data),
#endif
	.dev_data[AB5500_DEVID_ONSWA] = &ab5500_ponkey_data,
	.dev_data_sz[AB5500_DEVID_ONSWA] = sizeof(ab5500_ponkey_data),
	.dev_data[AB5500_DEVID_USB] = &abx500_usbgpio_plat_data,
	.dev_data_sz[AB5500_DEVID_USB] = sizeof(abx500_usbgpio_plat_data),
};

static struct platform_device u5500_ab5500_device = {
	.name = "ab5500-core",
	.id = 0,
	.dev = {
		.platform_data = &ab5500_plf_data,
	},
	.num_resources = 1,
	.resource = ab5500_resources,
};

static struct platform_device u5500_mloader_device = {
	.name = "db5500_mloader",
	.id = -1,
	.num_resources = 0,
};

static struct cryp_platform_data u5500_cryp1_platform_data = {
	.mem_to_engine = {
		.dir = STEDMA40_MEM_TO_PERIPH,
		.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
		.dst_dev_type = DB5500_DMA_DEV48_CRYPTO1_TX,
		.src_info.data_width = STEDMA40_WORD_WIDTH,
		.dst_info.data_width = STEDMA40_WORD_WIDTH,
		.mode = STEDMA40_MODE_LOGICAL,
		.src_info.psize = STEDMA40_PSIZE_LOG_4,
		.dst_info.psize = STEDMA40_PSIZE_LOG_4,
	},
	.engine_to_mem = {
		.dir = STEDMA40_PERIPH_TO_MEM,
		.src_dev_type = DB5500_DMA_DEV48_CRYPTO1_RX,
		.dst_dev_type = STEDMA40_DEV_DST_MEMORY,
		.src_info.data_width = STEDMA40_WORD_WIDTH,
		.dst_info.data_width = STEDMA40_WORD_WIDTH,
		.mode = STEDMA40_MODE_LOGICAL,
		.src_info.psize = STEDMA40_PSIZE_LOG_4,
		.dst_info.psize = STEDMA40_PSIZE_LOG_4,
	}
};

static struct hash_platform_data u5500_hash1_platform_data = {
	.mem_to_engine = {
.dir = STEDMA40_MEM_TO_PERIPH,
.src_dev_type = STEDMA40_DEV_SRC_MEMORY,
.dst_dev_type = DB5500_DMA_DEV50_HASH1_TX,
.src_info.data_width = STEDMA40_WORD_WIDTH,
.dst_info.data_width = STEDMA40_WORD_WIDTH,
.mode = STEDMA40_MODE_LOGICAL,
.src_info.psize = STEDMA40_PSIZE_LOG_16,
.dst_info.psize = STEDMA40_PSIZE_LOG_16,
},
};

static struct platform_device *u5500_platform_devices[] __initdata = {
	&u5500_ab5500_device,
	&u5500_mcde_device,
	&ux500_hwmem_device,
	&u5500_b2r2_device,
	&u5500_mloader_device,
#ifdef CONFIG_U5500_MMIO
	&u5500_mmio_device,
#endif
	&u5500_thsens_device,
};

/*
 * This function check whether it is Small S5500 board
 * GPIO0 is HIGH for S5500
 */
bool is_s5500_board()
{
	int err , val ;

	err = gpio_request(GPIO_BOARD_VERSION, "Board Version");
	if (err) {
		pr_err("Error %d while requesting GPIO for Board Version\n",
				err);
		return err;
	}

	err = gpio_direction_input(GPIO_BOARD_VERSION);
	if (err) {
		pr_err("Error %d while setting GPIO for Board Version"
				"output mode\n", err);
		return err;
	}

	val = gpio_get_value(GPIO_BOARD_VERSION);

	gpio_free(GPIO_BOARD_VERSION);

	return (val == 1);
}


static void __init u5500_i2c_init(void)
{
	db5500_add_i2c1(&u5500_i2c1_data);
	db5500_add_i2c2(&u5500_i2c2_data);
	db5500_add_i2c3(&u5500_i2c3_data);

	i2c_register_board_info(1, ARRAY_AND_SIZE(u5500_i2c1_devices));
	i2c_register_board_info(2, ARRAY_AND_SIZE(u5500_i2c2_devices));

	if (cpu_is_u5500v1())
		i2c_register_board_info(2, ARRAY_AND_SIZE(u5500v1_i2c2_sensor_devices));

	if (cpu_is_u5500v2()) {
		/*
		 * In V2 display is mounted in reverse direction,
		 * so need to change the intial
		 * settings of Accelerometer and Magnetometer
		 */
		lsm303dlh_pdata.negative_x = 1;
		lsm303dlh_pdata.negative_y = 1;
		i2c_register_board_info(2, ARRAY_AND_SIZE(u5500v2_i2c2_sensor_devices));
	}
}

static void __init u5500_uart_init(void)
{
	db5500_add_uart0(NULL);
	db5500_add_uart1(NULL);
	db5500_add_uart2(NULL);
	db5500_add_uart3(NULL);
}

static void __init u5500_cryp1_hash1_init(void)
{
	db5500_add_cryp1(&u5500_cryp1_platform_data);
	db5500_add_hash1(&u5500_hash1_platform_data);
}

static void __init u5500_init_machine(void)
{
	u5500_regulators_init();
	u5500_init_devices();
	u5500_pins_init();

	u5500_i2c_init();
	u5500_msp_init();
	u5500_spi_init();

	u5500_sdi_init();
	u5500_uart_init();

	u5500_wlan_init();

	db5500_add_keypad(&u5500_keypad_board);
	u5500_cryp1_hash1_init();

#ifdef CONFIG_TOUCHSCREEN_CYTTSP_SPI
	u5500_cyttsp_init();
#endif

	platform_add_devices(u5500_platform_devices,
			     ARRAY_SIZE(u5500_platform_devices));
}

MACHINE_START(U5500, "ST-Ericsson U5500 Platform")
	.atag_offset	= 0x100,
	.map_io		= u5500_map_io,
	.init_irq	= ux500_init_irq,
	.timer		= &ux500_timer,
	.init_machine	= u5500_init_machine,
MACHINE_END

MACHINE_START(B5500, "ST-Ericsson U5500 Big Board")
	.atag_offset	= 0x00000100,
	.map_io		= u5500_map_io,
	.init_irq	= ux500_init_irq,
	.timer		= &ux500_timer,
	.init_machine	= u5500_init_machine,
MACHINE_END
