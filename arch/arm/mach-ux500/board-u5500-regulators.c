/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/ab5500.h>

#include "regulator-u5500.h"
#include "board-u5500.h"

/*
 * AB5500
 */

static struct regulator_consumer_supply ab5500_ldo_g_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdi1"),
};

static struct regulator_consumer_supply ab5500_ldo_h_consumers[] = {
	REGULATOR_SUPPLY("v-display", NULL),
	REGULATOR_SUPPLY("vdd", "1-004b"), /* Synaptics */
	REGULATOR_SUPPLY("vin", "2-0036"), /* LM3530 */
	REGULATOR_SUPPLY("vcpin", "spi1.0"),
};

static struct regulator_consumer_supply ab5500_ldo_k_consumers[] = {
	REGULATOR_SUPPLY("vdd", "lsm303dlh.0"),
	REGULATOR_SUPPLY("vdd", "lsm303dlh.1"),
	REGULATOR_SUPPLY("v-mmio-camera", "mmio_camera"),
};

static struct regulator_consumer_supply ab5500_ldo_l_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdi0"),
	REGULATOR_SUPPLY("vmmc", "sdi2"),
};

static struct regulator_consumer_supply ab5500_ldo_vdigmic_consumers[] = {
};

static struct regulator_consumer_supply ab5500_ldo_sim_consumers[] = {
	REGULATOR_SUPPLY("debug", "reg-virt-consumer.5"),
};

static struct regulator_init_data
ab5500_regulator_init_data[AB5500_NUM_REGULATORS] = {
	/* SD Card */
	[AB5500_LDO_G] = {
		.constraints = {
			.min_uV		= 1200000,
			.max_uV		= 2910000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS |
					  REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
					    REGULATOR_MODE_IDLE,
		},
		.consumer_supplies	= ab5500_ldo_g_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_g_consumers),
	},
	/* Display */
	[AB5500_LDO_H] = {
		.constraints = {
			.min_uV		= 2790000,
			.max_uV		= 2790000,
			.apply_uV	= 1,
			.boot_on	= 1, /* display on during boot */
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_h_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_h_consumers),
	},
	/* Camera */
	[AB5500_LDO_K] = {
		.constraints = {
			.min_uV		= 2790000,
			.max_uV		= 2790000,
			.apply_uV	= 1,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_k_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_k_consumers),
	},
	/* External eMMC */
	[AB5500_LDO_L] = {
		.constraints = {
			.min_uV		= 1200000,
			.max_uV		= 2910000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS |
					  REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
					    REGULATOR_MODE_IDLE,
		},
		.consumer_supplies	= ab5500_ldo_l_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_l_consumers),
	},
	[AB5500_LDO_VDIGMIC] = {
		.constraints = {
			.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_vdigmic_consumers,
		.num_consumer_supplies	=
			ARRAY_SIZE(ab5500_ldo_vdigmic_consumers),
	},
	[AB5500_LDO_SIM] = {
		.constraints = {
			.boot_on	= 1,
			.always_on	= 1,
			.min_uV		= 2900000,
			.max_uV		= 2900000,
			.apply_uV	= 1,
			.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_sim_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_sim_consumers),
	},
};

struct ab5500_regulator_platform_data u5500_ab5500_regulator_data = {
	.regulator	= ab5500_regulator_init_data,
	.num_regulator	= ARRAY_SIZE(ab5500_regulator_init_data),
};

/*
 * Power state, ePOD, etc.
 */

static struct regulator_consumer_supply u5500_vape_consumers[] = {
	REGULATOR_SUPPLY("v-ape", NULL),
	REGULATOR_SUPPLY("v-i2c", "nmk-i2c.0"),
	REGULATOR_SUPPLY("v-i2c", "nmk-i2c.1"),
	REGULATOR_SUPPLY("v-i2c", "nmk-i2c.2"),
	REGULATOR_SUPPLY("v-i2c", "nmk-i2c.3"),
	REGULATOR_SUPPLY("vcore", "sdi0"),
	REGULATOR_SUPPLY("vcore", "sdi1"),
	REGULATOR_SUPPLY("vcore", "sdi2"),
	REGULATOR_SUPPLY("vcore", "sdi3"),
	REGULATOR_SUPPLY("vcore", "sdi4"),
	REGULATOR_SUPPLY("v-uart", "uart0"),
	REGULATOR_SUPPLY("v-uart", "uart1"),
	REGULATOR_SUPPLY("v-uart", "uart2"),
	REGULATOR_SUPPLY("v-uart", "uart3"),
	REGULATOR_SUPPLY("v-ape", "db5500-keypad"),
};

static struct regulator_consumer_supply u5500_sga_consumers[] = {
	REGULATOR_SUPPLY("debug", "reg-virt-consumer.0"),
	REGULATOR_SUPPLY("v-mali", NULL),
};

static struct regulator_consumer_supply u5500_hva_consumers[] = {
	REGULATOR_SUPPLY("debug", "reg-virt-consumer.1"),
	REGULATOR_SUPPLY("v-hva", NULL),
};

static struct regulator_consumer_supply u5500_sia_consumers[] = {
	REGULATOR_SUPPLY("debug", "reg-virt-consumer.2"),
	REGULATOR_SUPPLY("v-sia", "mmio_camera"),
};

static struct regulator_consumer_supply u5500_disp_consumers[] = {
	REGULATOR_SUPPLY("debug", "reg-virt-consumer.3"),
	REGULATOR_SUPPLY("vsupply", "b2r2_bus"),
	REGULATOR_SUPPLY("vsupply", "mcde"),
};

static struct regulator_consumer_supply u5500_esram12_consumers[] = {
	REGULATOR_SUPPLY("debug", "reg-virt-consumer.4"),
	REGULATOR_SUPPLY("v-esram12", "mcde"),
	REGULATOR_SUPPLY("esram12", "hva"),
};

#define U5500_REGULATOR_SWITCH(lower, upper)                            \
[U5500_REGULATOR_SWITCH_##upper] = (struct regulator_init_data []) {    \
{                                                                       \
	.constraints = {                                                \
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,              \
	},                                                              \
	.consumer_supplies      = u5500_##lower##_consumers,            \
	.num_consumer_supplies  = ARRAY_SIZE(u5500_##lower##_consumers),\
}                                                                       \
}

/*
 * EPODs that require VAPE as supply regulator
 */

#define U5500_REGULATOR_SWITCH_WITH_SUPPLY(lower, upper)                \
[U5500_REGULATOR_SWITCH_##upper] = (struct regulator_init_data []) {    \
{                                                                       \
	.supply_regulator = "u5500-vape",                               \
	.constraints = {                                                \
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,              \
	},                                                              \
	.consumer_supplies      = u5500_##lower##_consumers,            \
	.num_consumer_supplies  = ARRAY_SIZE(u5500_##lower##_consumers),\
}                                                                       \
}

static struct regulator_init_data *
u5500_regulator_init_data[U5500_NUM_REGULATORS] __initdata = {
	[U5500_REGULATOR_VAPE] = (struct regulator_init_data []) {
		{
			.constraints = {
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
			},
			.consumer_supplies	= u5500_vape_consumers,
			.num_consumer_supplies	= ARRAY_SIZE(u5500_vape_consumers),
		}
	},
	U5500_REGULATOR_SWITCH_WITH_SUPPLY(sga, SGA),
	U5500_REGULATOR_SWITCH_WITH_SUPPLY(hva, HVA),
	U5500_REGULATOR_SWITCH_WITH_SUPPLY(sia, SIA),
	U5500_REGULATOR_SWITCH_WITH_SUPPLY(disp, DISP),
	/*
	 * ESRAM12 is put in rentention by FW on VAPE off
	 * no need to hold VAPE
	 */
	U5500_REGULATOR_SWITCH(esram12, ESRAM12),
};

static void __init u5500_regulators_init_debug(void)
{
	const char data[] = "debug";
	int i;

	for (i = 0; i < 6; i++)
		platform_device_register_data(NULL, "reg-virt-consumer", i,
			data, sizeof(data));
}

static struct regulator_consumer_supply u5500_vio_consumers[] = {
	REGULATOR_SUPPLY("gbf_1v8", "cg2900-uart.0"),
};

static struct regulator_init_data u5500_vio_init_data = {
	.constraints.always_on	= 1,
	.consumer_supplies	= u5500_vio_consumers,
	.num_consumer_supplies	= ARRAY_SIZE(u5500_vio_consumers),
};

static struct fixed_voltage_config u5500_vio_pdata __initdata = {
	.supply_name	= "vio_1v8",
	.microvolts	= 1800000,
	.init_data	= &u5500_vio_init_data,
	.gpio		= -EINVAL,
};

void __init u5500_regulators_init(void)
{
	u5500_regulators_init_debug();

	platform_device_register_data(NULL, "u5500-regulators", -1,
			u5500_regulator_init_data,
			sizeof(u5500_regulator_init_data));

	platform_device_register_data(NULL, "reg-fixed-voltage", -1,
				      &u5500_vio_pdata,
				      sizeof(u5500_vio_pdata));

	regulator_has_full_constraints();
}
