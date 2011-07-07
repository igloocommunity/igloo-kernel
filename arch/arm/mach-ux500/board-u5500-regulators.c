/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/ab5500.h>

#include "board-u5500.h"

/*
 * AB5500
 */

static struct regulator_consumer_supply ab5500_ldo_d_consumers[] = {
};

static struct regulator_consumer_supply ab5500_ldo_g_consumers[] = {
	REGULATOR_SUPPLY("v-MMC-SD", "sdi1"),
};

static struct regulator_consumer_supply ab5500_ldo_h_consumers[] = {
	REGULATOR_SUPPLY("v-display", NULL),
	REGULATOR_SUPPLY("vdd", "1-004b"), /* Synaptics */
	REGULATOR_SUPPLY("vin", "2-0036"), /* LM3530 */
};

static struct regulator_consumer_supply ab5500_ldo_k_consumers[] = {
	REGULATOR_SUPPLY("v-accel", "lsm303dlh.0"),
	REGULATOR_SUPPLY("v-mag", "lsm303dlh.1"),
	REGULATOR_SUPPLY("v-mmio-camera", "mmio_camera"),
};

static struct regulator_consumer_supply ab5500_ldo_l_consumers[] = {
};

static struct regulator_consumer_supply ab5500_ldo_s_consumers[] = {
	REGULATOR_SUPPLY("v-ana", "mcde"),
	REGULATOR_SUPPLY("v-ana", "mmio_camera"),
};

static struct regulator_consumer_supply ab5500_ldo_vdigmic_consumers[] = {
};

static struct regulator_consumer_supply ab5500_ldo_sim_consumers[] = {
};

static struct regulator_init_data
ab5500_regulator_init_data[AB5500_NUM_REGULATORS] = {
	/* AB internal analog */
	[AB5500_LDO_D] = {
		.constraints = {
			.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_d_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_d_consumers),
	},
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
					  REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_l_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_l_consumers),
	},
	[AB5500_LDO_S] = {
		.constraints = {
			.name		= "VANA",
			.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		},
		.consumer_supplies	= ab5500_ldo_s_consumers,
		.num_consumer_supplies	= ARRAY_SIZE(ab5500_ldo_s_consumers),
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

static void __init u5500_regulators_init_debug(void)
{
	const char data[] = "debug";
	int i;

	for (i = 0; i < 4; i++)
		platform_device_register_data(NULL, "reg-virt-consumer", i,
			data, sizeof(data));
}

void __init u5500_regulators_init(void)
{
	u5500_regulators_init_debug();
}
