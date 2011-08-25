/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com> for ST-Ericsson
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/prcmu.h>

#include "regulator-ux500.h"
#include "regulator-u8500.h"

static struct u8500_regulator_info
		u8500_regulator_info[U8500_NUM_REGULATORS] = {
	[U8500_REGULATOR_VAPE] = {
		.desc = {
			.name	= "u8500-vape",
			.id	= U8500_REGULATOR_VAPE,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_VARM] = {
		.desc = {
			.name	= "u8500-varm",
			.id	= U8500_REGULATOR_VARM,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_VMODEM] = {
		.desc = {
			.name	= "u8500-vmodem",
			.id	= U8500_REGULATOR_VMODEM,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_VPLL] = {
		.desc = {
			.name	= "u8500-vpll",
			.id	= U8500_REGULATOR_VPLL,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_VSMPS1] = {
		.desc = {
			.name	= "u8500-vsmps1",
			.id	= U8500_REGULATOR_VSMPS1,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_VSMPS2] = {
		.desc = {
			.name	= "u8500-vsmps2",
			.id	= U8500_REGULATOR_VSMPS2,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.exclude_from_power_state = true,
	},
	[U8500_REGULATOR_VSMPS3] = {
		.desc = {
			.name	= "u8500-vsmps3",
			.id	= U8500_REGULATOR_VSMPS3,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_VRF1] = {
		.desc = {
			.name	= "u8500-vrf1",
			.id	= U8500_REGULATOR_VRF1,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[U8500_REGULATOR_SWITCH_SVAMMDSP] = {
		.desc = {
			.name	= "u8500-sva-mmdsp",
			.id	= U8500_REGULATOR_SWITCH_SVAMMDSP,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SVAMMDSP,
	},
	[U8500_REGULATOR_SWITCH_SVAMMDSPRET] = {
		.desc = {
			.name	= "u8500-sva-mmdsp-ret",
			.id	= U8500_REGULATOR_SWITCH_SVAMMDSPRET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SVAMMDSP,
		.is_ramret = true,
	},
	[U8500_REGULATOR_SWITCH_SVAPIPE] = {
		.desc = {
			.name	= "u8500-sva-pipe",
			.id	= U8500_REGULATOR_SWITCH_SVAPIPE,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SVAPIPE,
	},
	[U8500_REGULATOR_SWITCH_SIAMMDSP] = {
		.desc = {
			.name	= "u8500-sia-mmdsp",
			.id	= U8500_REGULATOR_SWITCH_SIAMMDSP,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SIAMMDSP,
	},
	[U8500_REGULATOR_SWITCH_SIAMMDSPRET] = {
		.desc = {
			.name	= "u8500-sia-mmdsp-ret",
			.id	= U8500_REGULATOR_SWITCH_SIAMMDSPRET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SIAMMDSP,
		.is_ramret = true,
	},
	[U8500_REGULATOR_SWITCH_SIAPIPE] = {
		.desc = {
			.name	= "u8500-sia-pipe",
			.id	= U8500_REGULATOR_SWITCH_SIAPIPE,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SIAPIPE,
	},
	[U8500_REGULATOR_SWITCH_SGA] = {
		.desc = {
			.name	= "u8500-sga",
			.id	= U8500_REGULATOR_SWITCH_SGA,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SGA,
	},
	[U8500_REGULATOR_SWITCH_B2R2_MCDE] = {
		.desc = {
			.name	= "u8500-b2r2-mcde",
			.id	= U8500_REGULATOR_SWITCH_B2R2_MCDE,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_B2R2_MCDE,
	},
	[U8500_REGULATOR_SWITCH_ESRAM12] = {
		.desc = {
			.name	= "u8500-esram12",
			.id	= U8500_REGULATOR_SWITCH_ESRAM12,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id	= EPOD_ID_ESRAM12,
		.is_enabled	= true,
	},
	[U8500_REGULATOR_SWITCH_ESRAM12RET] = {
		.desc = {
			.name	= "u8500-esram12-ret",
			.id	= U8500_REGULATOR_SWITCH_ESRAM12RET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_ESRAM12,
		.is_ramret = true,
	},
	[U8500_REGULATOR_SWITCH_ESRAM34] = {
		.desc = {
			.name	= "u8500-esram34",
			.id	= U8500_REGULATOR_SWITCH_ESRAM34,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id	= EPOD_ID_ESRAM34,
		.is_enabled	= true,
	},
	[U8500_REGULATOR_SWITCH_ESRAM34RET] = {
		.desc = {
			.name	= "u8500-esram34-ret",
			.id	= U8500_REGULATOR_SWITCH_ESRAM34RET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_ESRAM34,
		.is_ramret = true,
	},
};

static int __devinit u8500_regulator_probe(struct platform_device *pdev)
{
	int ret;

	ret = ux500_regulator_probe(pdev, u8500_regulator_info,
				    ARRAY_SIZE(u8500_regulator_info));
	if (!ret)
		regulator_has_full_constraints();

	return ret;
}

static int __devexit u8500_regulator_remove(struct platform_device *pdev)
{
	return ux500_regulator_remove(pdev, u8500_regulator_info,
				      ARRAY_SIZE(u8500_regulator_info));
}

static struct platform_driver u8500_regulator_driver = {
	.driver = {
		.name = "u8500-regulators",
		.owner = THIS_MODULE,
	},
	.probe	= u8500_regulator_probe,
	.remove = __devexit_p(u8500_regulator_remove),
};

static int __init u8500_regulator_init(void)
{
	return platform_driver_register(&u8500_regulator_driver);
}

static void __exit u8500_regulator_exit(void)
{
	platform_driver_unregister(&u8500_regulator_driver);
}

/* replaced subsys_initcall as regulators must be turned on early */
arch_initcall(u8500_regulator_init);
module_exit(u8500_regulator_exit);

MODULE_AUTHOR("Bengt Jonsson <bengt.g.jonsson@stericsson.com>");
MODULE_DESCRIPTION("U8500 regulator driver");
MODULE_LICENSE("GPL v2");
