/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com> for ST-Ericsson
 *
 * Board specific file for configuration of virtual regulators. These virtual
 * regulators are used for debug purposes. They connect to the regulator device
 * just like any other consumer and expose controls in sysfs, so that
 * regulators can be controlled from user space.
 */

#include <linux/init.h>
#include <linux/platform_device.h>

/*
 * Configuration for AB8500 virtual regulators
 */
static struct platform_device u8500_aux1_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 0,
	.dev = {
		.platform_data = "aux1",
	},
};

static struct platform_device u8500_aux2_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 1,
	.dev = {
		.platform_data = "aux2",
	},
};

static struct platform_device u8500_aux3_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 2,
	.dev = {
		.platform_data = "aux3",
	},
};

static struct platform_device u8500_intcore_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 3,
	.dev = {
		.platform_data = "intcore",
	},
};

static struct platform_device u8500_tvout_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 4,
	.dev = {
		.platform_data = "tvout",
	},
};

static struct platform_device u8500_audio_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 5,
	.dev = {
		.platform_data = "audio",
	},
};

static struct platform_device u8500_anamic1_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 6,
	.dev = {
		.platform_data = "anamic1",
	},
};

static struct platform_device u8500_anamic2_virtual_regulator_device  = {
	.name = "reg-virt-consumer",
	.id = 7,
	.dev = {
		.platform_data = "anamic2",
	},
};

static struct platform_device u8500_dmic_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 8,
	.dev = {
		.platform_data = "dmic",
	},
};

static struct platform_device u8500_ana_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 9,
	.dev = {
		.platform_data = "ana",
	},
};

static struct platform_device u8500_sysclkreq_2_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 10,
	.dev = {
		.platform_data = "sysclkreq-2",
	},
};

static struct platform_device u8500_sysclkreq_4_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 11,
	.dev = {
		.platform_data = "sysclkreq-4",
	},
};

/*
 * Configuration for other U8500 virtual regulators
 */
static struct platform_device u8500_ape_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 12,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_arm_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 13,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_modem_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 14,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_pll_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 15,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_smps1_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 16,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_smps2_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 17,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_smps3_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 18,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_rf1_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 19,
	.dev = {
		.platform_data = "test",
	},
};

/*
 * Configuration for U8500 power domain virtual regulators
 */
static struct platform_device u8500_sva_mmdsp_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 20,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_sva_mmdsp_ret_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 21,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_sva_pipe_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 22,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_sia_mmdsp_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 23,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_sia_mmdsp_ret_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 24,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_sia_pipe_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 25,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_sga_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 26,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_b2r2_mcde_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 27,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_esram12_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 28,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_esram12_ret_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 29,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_esram34_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 30,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device u8500_esram34_ret_virtual_regulator_device = {
	.name = "reg-virt-consumer",
	.id = 31,
	.dev = {
		.platform_data = "test",
	},
};

static struct platform_device *u8500_virtual_regulator_devices[] = {
	&u8500_aux1_virtual_regulator_device,
	&u8500_aux2_virtual_regulator_device,
	&u8500_aux3_virtual_regulator_device,
	&u8500_intcore_virtual_regulator_device,
	&u8500_tvout_virtual_regulator_device,
	&u8500_audio_virtual_regulator_device,
	&u8500_anamic1_virtual_regulator_device,
	&u8500_anamic2_virtual_regulator_device,
	&u8500_dmic_virtual_regulator_device,
	&u8500_ana_virtual_regulator_device,
	&u8500_sysclkreq_2_virtual_regulator_device,
	&u8500_sysclkreq_4_virtual_regulator_device,
	&u8500_ape_virtual_regulator_device,
	&u8500_arm_virtual_regulator_device,
	&u8500_modem_virtual_regulator_device,
	&u8500_pll_virtual_regulator_device,
	&u8500_smps1_virtual_regulator_device,
	&u8500_smps2_virtual_regulator_device,
	&u8500_smps3_virtual_regulator_device,
	&u8500_rf1_virtual_regulator_device,
	&u8500_sva_mmdsp_virtual_regulator_device,
	&u8500_sva_mmdsp_ret_virtual_regulator_device,
	&u8500_sva_pipe_virtual_regulator_device,
	&u8500_sia_mmdsp_virtual_regulator_device,
	&u8500_sia_mmdsp_ret_virtual_regulator_device,
	&u8500_sia_pipe_virtual_regulator_device,
	&u8500_sga_virtual_regulator_device,
	&u8500_b2r2_mcde_virtual_regulator_device,
	&u8500_esram12_virtual_regulator_device,
	&u8500_esram12_ret_virtual_regulator_device,
	&u8500_esram34_virtual_regulator_device,
	&u8500_esram34_ret_virtual_regulator_device,
};

static int __init u8500_virtual_regulator_init(void)
{
	int ret;

	ret = platform_add_devices(u8500_virtual_regulator_devices,
			ARRAY_SIZE(u8500_virtual_regulator_devices));
	if (ret != 0)
		pr_err("Failed to register U8500 virtual regulator devices:"
			" %d\n", ret);

	return ret;
}
module_init(u8500_virtual_regulator_init);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Bengt Jonsson <bengt.g.jonsson@stericsson.com");
MODULE_DESCRIPTION("Configuration of u8500 virtual regulators");
MODULE_ALIAS("platform:u8500-virtual-regulator");
