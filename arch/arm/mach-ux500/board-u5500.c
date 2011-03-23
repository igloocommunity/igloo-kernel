/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Rabin Vincent <rabin.vincent@stericsson.com> for ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/led-lm3530.h>
#include <linux/input/matrix_keypad.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/i2c.h>

#include <mach/hardware.h>
#include <mach/devices.h>
#include <mach/setup.h>
#include <mach/db5500-keypad.h>

#include "devices-db5500.h"

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
	.brt_ramp_fall = LM3530_RAMP_TIME_1ms,
	.brt_ramp_rise = LM3530_RAMP_TIME_1ms,
	.als1_resistor_sel = LM3530_ALS_IMPD_2_27kOhm,
	.als2_resistor_sel = LM3530_ALS_IMPD_2_27kOhm,
	.brt_val = 0x7F,	/* Max brightness */
};

/*
 * I2C
 */

#define U5500_I2C_CONTROLLER(id, _slsu, _tft, _rft, clk, _sm) \
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
	.sm		= _sm,		\
}

/*
 * The board uses 3 i2c controllers, initialize all of
 * them with slave data setup time of 250 ns,
 * Tx & Rx FIFO threshold values as 1 and standard
 * mode of operation
 */

U5500_I2C_CONTROLLER(1,	0xe, 1, 1, 400000, I2C_FREQ_MODE_FAST);
U5500_I2C_CONTROLLER(2,	0xe, 1, 1, 400000, I2C_FREQ_MODE_FAST);
U5500_I2C_CONTROLLER(3,	0xe, 1, 1, 400000, I2C_FREQ_MODE_FAST);

static struct i2c_board_info __initdata u5500_i2c1_devices[] = {
};

static struct i2c_board_info __initdata u5500_i2c2_devices[] = {
	{
		/* Backlight */
		I2C_BOARD_INFO("lm3530-led", 0x36),
		.platform_data = &u5500_als_platform_data,
	},
};

static void __init u5500_i2c_init(void)
{
	db5500_add_i2c1(&u5500_i2c1_data);
	db5500_add_i2c2(&u5500_i2c2_data);
	db5500_add_i2c3(&u5500_i2c3_data);

	i2c_register_board_info(1, ARRAY_AND_SIZE(u5500_i2c1_devices));
	i2c_register_board_info(2, ARRAY_AND_SIZE(u5500_i2c2_devices));
}

/*
 * Keypad
 */

static const unsigned int u5500_keymap[] = {
	KEY(4, 0, KEY_CAMERA), /* Camera2 */
	KEY(4, 1, KEY_CAMERA_FOCUS), /* Camera1 */
	KEY(4, 2, KEY_MENU),
	KEY(4, 3, KEY_BACK),
	KEY(5, 2, KEY_SEND),
	KEY(5, 3, KEY_HOME),
	KEY(8, 0, KEY_END),
	KEY(8, 1, KEY_VOLUMEUP),
	KEY(8, 2, KEY_VOLUMEDOWN),
};

static struct matrix_keymap_data u5500_keymap_data = {
	.keymap		= u5500_keymap,
	.keymap_size	= ARRAY_SIZE(u5500_keymap),
};

static struct db5500_keypad_platform_data u5500_keypad_board = {
	.keymap_data	= &u5500_keymap_data,
	.no_autorepeat	= true,
	.debounce_ms	= 40, /* milliseconds */
};

static void __init u5500_uart_init(void)
{
	db5500_add_uart0(NULL);
	db5500_add_uart1(NULL);
	db5500_add_uart2(NULL);
}

static void __init u5500_init_machine(void)
{
	u5500_init_devices();
	u5500_i2c_init();

	u5500_sdi_init();
	u5500_uart_init();

	db5500_add_keypad(&u5500_keypad_board);
}

MACHINE_START(U5500, "ST-Ericsson U5500 Platform")
	.boot_params	= 0x00000100,
	.map_io		= u5500_map_io,
	.init_irq	= ux500_init_irq,
	.timer		= &ux500_timer,
	.init_machine	= u5500_init_machine,
MACHINE_END
