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
#include <linux/input/matrix_keypad.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <mach/devices.h>
#include <mach/setup.h>
#include <mach/db5500-keypad.h>

#include "devices-db5500.h"

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
