/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License terms: GNU General Public License (GPL), version 2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mfd/stmpe.h>
#include <linux/input/bu21013.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/lsm303dlh.h>
#include <linux/l3g4200d.h>
#include <linux/i2c.h>
#include <linux/i2c/adp1653_plat.h>
#include <linux/input/matrix_keypad.h>
#include <linux/input/lps001wp.h>
#include <asm/mach-types.h>

#include "board-mop500.h"

/*
 * LSM303DLH accelerometer + magnetometer sensors
 */
static struct lsm303dlh_platform_data __initdata lsm303dlh_pdata= {
	.name_a = "lsm303dlh.0",
	.name_m = "lsm303dlh.1",
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negative_x = 1,
	.negative_y = 1,
	.negative_z = 0,
};

static struct l3g4200d_gyr_platform_data  __initdata l3g4200d_pdata_u8500 = {
	.name_gyr = "l3g4200d",
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negative_x = 0,
	.negative_y = 0,
	.negative_z = 1,
};

static struct lps001wp_prs_platform_data __initdata lps001wp_pdata = {
	.poll_interval = 500,
	.min_interval = 10,
};

static struct adp1653_platform_data __initdata adp1653_pdata_u8500_uib = {
	.irq_no = CAMERA_FLASH_INT_PIN
};

static struct i2c_board_info __initdata mop500_i2c2_devices[] = {
	{
		/* LSM303DLH Magnetometer */
		I2C_BOARD_INFO("lsm303dlh_m", 0x1E),
		.platform_data = &lsm303dlh_pdata,
	},
	{
		/* L3G4200D Gyroscope */
		I2C_BOARD_INFO("l3g4200d", 0x68),
		.platform_data = &l3g4200d_pdata_u8500,
	},
	{
		/* LSP001WM Barometer */
		I2C_BOARD_INFO("lps001wp_prs_sysfs", 0x5C),
		.platform_data = &lps001wp_pdata,
	},
	{
		I2C_BOARD_INFO("adp1653", 0x30),
		.platform_data = &adp1653_pdata_u8500_uib
	}
};

/*
 * Break this out due to the fact that this have changed address on Snowball
 */
static struct i2c_board_info __initdata mop500_2_i2c2_devices[] = {
       {
               /* LSM303DLH Accelerometer */
               I2C_BOARD_INFO("lsm303dlh_a", 0x18),
               .platform_data = &lsm303dlh_pdata,
       },
};

/*
 * This is needed due to the fact that the i2c address changed in Snowball V7 =<
 * and there is no way of knowing if the HW is Snowball V7 or higher so we just
 * have to try and fail.
 */
static struct i2c_board_info __initdata snowball_i2c2_devices[] = {
       {
               /* LSM303DLH Accelerometer */
               I2C_BOARD_INFO("lsm303dlh_a", 0x19),
               .platform_data = &lsm303dlh_pdata,
       },
};

/*
 * ux500 keymaps
 *
 * Organized row-wise as on the UIB, starting at the top-left
 *
 * we support two key layouts, specific to requirements. The first
 * keylayout includes controls for power/volume a few generic keys;
 * the second key layout contains the full numeric layout, enter/back/left
 * buttons along with a "."(dot), specifically for connectivity testing
 */
static const unsigned int mop500_keymap[] = {
#if defined(CONFIG_KEYLAYOUT_LAYOUT1)
	KEY(2, 5, KEY_END),
	KEY(4, 1, KEY_HOME),
	KEY(3, 5, KEY_VOLUMEDOWN),
	KEY(1, 3, KEY_EMAIL),
	KEY(5, 2, KEY_RIGHT),
	KEY(5, 0, KEY_BACKSPACE),

	KEY(0, 5, KEY_MENU),
	KEY(7, 6, KEY_ENTER),
	KEY(4, 5, KEY_0),
	KEY(6, 7, KEY_DOT),
	KEY(3, 4, KEY_UP),
	KEY(3, 3, KEY_DOWN),

	KEY(6, 4, KEY_SEND),
	KEY(6, 2, KEY_BACK),
	KEY(4, 2, KEY_VOLUMEUP),
	KEY(5, 5, KEY_SPACE),
	KEY(4, 3, KEY_LEFT),
	KEY(3, 2, KEY_SEARCH),
#elif defined(CONFIG_KEYLAYOUT_LAYOUT2)
	KEY(2, 5, KEY_RIGHT),
	KEY(4, 1, KEY_ENTER),
	KEY(3, 5, KEY_MENU),
	KEY(1, 3, KEY_3),
	KEY(5, 2, KEY_6),
	KEY(5, 0, KEY_9),

	KEY(0, 5, KEY_UP),
	KEY(7, 6, KEY_DOWN),
	KEY(4, 5, KEY_0),
	KEY(6, 7, KEY_2),
	KEY(3, 4, KEY_5),
	KEY(3, 3, KEY_8),

	KEY(6, 4, KEY_LEFT),
	KEY(6, 2, KEY_BACK),
	KEY(4, 2, KEY_KPDOT),
	KEY(5, 5, KEY_1),
	KEY(4, 3, KEY_4),
	KEY(3, 2, KEY_7),
#else
#warning "No keypad layout defined."
#endif
};

static const struct matrix_keymap_data mop500_keymap_data = {
	.keymap		= mop500_keymap,
	.keymap_size    = ARRAY_SIZE(mop500_keymap),
};
/*
 * STMPE1601
 */
static struct stmpe_keypad_platform_data stmpe1601_keypad_data = {
	.debounce_ms    = 64,
	.scan_count     = 8,
	.no_autorepeat  = true,
	.keymap_data    = &mop500_keymap_data,
};

static struct stmpe_platform_data stmpe1601_data = {
	.id		= 1,
	.blocks		= STMPE_BLOCK_KEYPAD,
	.irq_trigger    = IRQF_TRIGGER_FALLING,
	.irq_base       = MOP500_STMPE1601_IRQ(0),
	.keypad		= &stmpe1601_keypad_data,
	.autosleep      = true,
	.autosleep_timeout = 1024,
};

static struct i2c_board_info __initdata mop500_i2c0_devices_stuib[] = {
	{
		I2C_BOARD_INFO("stmpe1601", 0x40),
		.irq = NOMADIK_GPIO_TO_IRQ(218),
		.platform_data = &stmpe1601_data,
		.flags = I2C_CLIENT_WAKE,
	},
};

/*
 * Register/Add i2c sensors for Snowball
 */
void mop500_sensors_probe_add_lsm303dlh_a(void)
{
       static const int busnum = 2;
       struct i2c_adapter *adap;
       struct i2c_client *client;
       static const unsigned short i2c_addr_list[] = {
               0x18, 0x19, I2C_CLIENT_END };
       struct i2c_board_info i2c_info = {
                       /* LSM303DLH Accelerometer */
                       I2C_BOARD_INFO("lsm303dlh_a", 0),
                       .platform_data = &lsm303dlh_pdata,
       };

       adap = i2c_get_adapter(busnum);
       if (!adap) {
               /* We have no i2c adapter yet lets create it. */
               pr_err(__FILE__ ": Could not get adapter %d\n", busnum);
               return;
       }
       client = i2c_new_probed_device(adap, &i2c_info,
                       i2c_addr_list, NULL);
       if (!client)
               pr_err(__FILE__ ": failed to register %s to i2c%d\n",
                               i2c_info.type,
                               busnum);
       i2c_put_adapter(adap);
}

/*
 * BU21013 ROHM touchscreen interface on the STUIBs
 */

/* tracks number of bu21013 devices being enabled */
static int bu21013_devices;

#define TOUCH_GPIO_PIN  84

#define TOUCH_XMAX	384
#define TOUCH_YMAX	704

#define PRCMU_CLOCK_OCR		0x1CC
#define TSC_EXT_CLOCK_9_6MHZ	0x840000

/**
 * bu21013_gpio_board_init : configures the touch panel.
 * @reset_pin: reset pin number
 * This function can be used to configures
 * the voltage and reset the touch panel controller.
 */
static int bu21013_gpio_board_init(int reset_pin)
{
	int retval = 0;

	bu21013_devices++;
	if (bu21013_devices == 1) {
		retval = gpio_request(reset_pin, "touchp_reset");
		if (retval) {
			printk(KERN_ERR "Unable to request gpio reset_pin");
			return retval;
		}
		retval = gpio_direction_output(reset_pin, 1);
		if (retval < 0) {
			printk(KERN_ERR "%s: gpio direction failed\n",
					__func__);
			return retval;
		}
		gpio_set_value_cansleep(reset_pin, 1);
	}

	return retval;
}

/**
 * bu21013_gpio_board_exit : deconfigures the touch panel controller
 * @reset_pin: reset pin number
 * This function can be used to deconfigures the chip selection
 * for touch panel controller.
 */
static int bu21013_gpio_board_exit(int reset_pin)
{
	int retval = 0;

	if (bu21013_devices == 1) {
		retval = gpio_direction_output(reset_pin, 0);
		if (retval < 0) {
			printk(KERN_ERR "%s: gpio direction failed\n",
					__func__);
			return retval;
		}
		gpio_set_value_cansleep(reset_pin, 0);
		gpio_free(reset_pin);
	}
	bu21013_devices--;

	return retval;
}

/**
 * bu21013_read_pin_val : get the interrupt pin value
 * This function can be used to get the interrupt pin value for touch panel
 * controller.
 */
static int bu21013_read_pin_val(void)
{
	return gpio_get_value(TOUCH_GPIO_PIN);
}

static struct bu21013_platform_device tsc_plat_device = {
	.cs_en = bu21013_gpio_board_init,
	.cs_dis = bu21013_gpio_board_exit,
	.irq_read_val = bu21013_read_pin_val,
	.irq = NOMADIK_GPIO_TO_IRQ(TOUCH_GPIO_PIN),
	.touch_x_max = TOUCH_XMAX,
	.touch_y_max = TOUCH_YMAX,
	.x_max_res = 480,
	.y_max_res = 864,
	.portrait = true,
	.has_ext_clk = true,
	.enable_ext_clk = false,
#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE) &&	\
		CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE == 270
	.x_flip		= true,
	.y_flip		= false,
#else
	.x_flip		= false,
	.y_flip		= true,
#endif
};

static struct bu21013_platform_device tsc_plat2_device = {
	.cs_en = bu21013_gpio_board_init,
	.cs_dis = bu21013_gpio_board_exit,
	.irq_read_val = bu21013_read_pin_val,
	.irq = NOMADIK_GPIO_TO_IRQ(TOUCH_GPIO_PIN),
	.touch_x_max = TOUCH_XMAX,
	.touch_y_max = TOUCH_YMAX,
	.x_max_res = 480,
	.y_max_res = 864,
	.portrait = true,
	.has_ext_clk = true,
	.enable_ext_clk = false,
#if defined(CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE) &&	\
		CONFIG_DISPLAY_GENERIC_DSI_PRIMARY_ROTATION_ANGLE == 270
	.x_flip		= true,
	.y_flip		= false,
#else
	.x_flip		= false,
	.y_flip		= true,
#endif
};

static struct i2c_board_info __initdata u8500_i2c3_devices_stuib[] = {
	{
		I2C_BOARD_INFO("bu21013_ts", 0x5C),
		.platform_data = &tsc_plat_device,
	},
	{
		I2C_BOARD_INFO("bu21013_ts", 0x5D),
		.platform_data = &tsc_plat2_device,
	},

};

void __init mop500_stuib_init(void)
{
	if (machine_is_hrefv60()) {
		tsc_plat_device.cs_pin = HREFV60_TOUCH_RST_GPIO;
		tsc_plat2_device.cs_pin = HREFV60_TOUCH_RST_GPIO;
		adp1653_pdata_u8500_uib.enable_gpio =
					HREFV60_CAMERA_FLASH_ENABLE;
	} else {
		tsc_plat_device.cs_pin = GPIO_BU21013_CS;
		tsc_plat2_device.cs_pin = GPIO_BU21013_CS;
		adp1653_pdata_u8500_uib.enable_gpio =
					GPIO_CAMERA_FLASH_ENABLE;
	}

	mop500_uib_i2c_add(0, mop500_i2c0_devices_stuib,
			ARRAY_SIZE(mop500_i2c0_devices_stuib));

	mop500_uib_i2c_add(3, u8500_i2c3_devices_stuib,
			ARRAY_SIZE(u8500_i2c3_devices_stuib));

	if (machine_is_hrefv60()) {
		lsm303dlh_pdata.irq_a1 = HREFV60_ACCEL_INT1_GPIO;
		lsm303dlh_pdata.irq_a2 = HREFV60_ACCEL_INT2_GPIO;
		lsm303dlh_pdata.irq_m = HREFV60_MAGNET_DRDY_GPIO;
	}
	else if (machine_is_snowball()) {
		lsm303dlh_pdata.irq_a1 = SNOWBALL_ACCEL_INT1_GPIO;
		lsm303dlh_pdata.irq_a2 = SNOWBALL_ACCEL_INT2_GPIO;
		lsm303dlh_pdata.irq_m = SNOWBALL_MAGNET_DRDY_GPIO;
	} else {
		lsm303dlh_pdata.irq_a1 = GPIO_ACCEL_INT1;
		lsm303dlh_pdata.irq_a2 = GPIO_ACCEL_INT2;
		lsm303dlh_pdata.irq_m = GPIO_MAGNET_DRDY;
	}
	mop500_uib_i2c_add(2, mop500_i2c2_devices,
			ARRAY_SIZE(mop500_i2c2_devices));


    if (machine_is_snowball()) {
        if (cpu_is_u8500v21()) {
            /* This is ugly but we cant know what address
             * to use */
			printk("%s: **** Try to add Accelometer and cpu_is_u8500v21 \n",
					__func__);
            mop500_sensors_probe_add_lsm303dlh_a();
        } else {/* Add the accelerometer with new addr */
			printk("%s: **** Try to add Accelometer old way \n",
					__func__);
            mop500_uib_i2c_add(2, snowball_i2c2_devices,
                           ARRAY_SIZE(snowball_i2c2_devices));
		}
    } else { /* none snowball have the old addr */
	     printk("%s: **** Ops on snowball but not a snowball Accelometer\n",
					__func__);
         mop500_uib_i2c_add(2, mop500_2_i2c2_devices,
                               ARRAY_SIZE(mop500_2_i2c2_devices));
	}
}
