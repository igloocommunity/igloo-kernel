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

#include <mach/prcmu-fw-api.h>

#include "regulator-ux500.h"
#include "regulator-db8500.h"

static struct u8500_regulator_info
		db8500_regulator_info[DB8500_NUM_REGULATORS] = {
	[DB8500_REGULATOR_VAPE] = {
		.desc = {
			.name	= "db8500-vape",
			.id	= DB8500_REGULATOR_VAPE,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_VARM] = {
		.desc = {
			.name	= "db8500-varm",
			.id	= DB8500_REGULATOR_VARM,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_VMODEM] = {
		.desc = {
			.name	= "db8500-vmodem",
			.id	= DB8500_REGULATOR_VMODEM,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_VPLL] = {
		.desc = {
			.name	= "db8500-vpll",
			.id	= DB8500_REGULATOR_VPLL,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_VSMPS1] = {
		.desc = {
			.name	= "db8500-vsmps1",
			.id	= DB8500_REGULATOR_VSMPS1,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_VSMPS2] = {
		.desc = {
			.name	= "db8500-vsmps2",
			.id	= DB8500_REGULATOR_VSMPS2,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.exclude_from_power_state = true,
	},
	[DB8500_REGULATOR_VSMPS3] = {
		.desc = {
			.name	= "db8500-vsmps3",
			.id	= DB8500_REGULATOR_VSMPS3,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_VRF1] = {
		.desc = {
			.name	= "db8500-vrf1",
			.id	= DB8500_REGULATOR_VRF1,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	[DB8500_REGULATOR_SWITCH_SVAMMDSP] = {
		.desc = {
			.name	= "db8500-sva-mmdsp",
			.id	= DB8500_REGULATOR_SWITCH_SVAMMDSP,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SVAMMDSP,
	},
	[DB8500_REGULATOR_SWITCH_SVAMMDSPRET] = {
		.desc = {
			.name	= "db8500-sva-mmdsp-ret",
			.id	= DB8500_REGULATOR_SWITCH_SVAMMDSPRET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SVAMMDSP,
		.is_ramret = true,
	},
	[DB8500_REGULATOR_SWITCH_SVAPIPE] = {
		.desc = {
			.name	= "db8500-sva-pipe",
			.id	= DB8500_REGULATOR_SWITCH_SVAPIPE,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SVAPIPE,
	},
	[DB8500_REGULATOR_SWITCH_SIAMMDSP] = {
		.desc = {
			.name	= "db8500-sia-mmdsp",
			.id	= DB8500_REGULATOR_SWITCH_SIAMMDSP,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SIAMMDSP,
	},
	[DB8500_REGULATOR_SWITCH_SIAMMDSPRET] = {
		.desc = {
			.name	= "db8500-sia-mmdsp-ret",
			.id	= DB8500_REGULATOR_SWITCH_SIAMMDSPRET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SIAMMDSP,
		.is_ramret = true,
	},
	[DB8500_REGULATOR_SWITCH_SIAPIPE] = {
		.desc = {
			.name	= "db8500-sia-pipe",
			.id	= DB8500_REGULATOR_SWITCH_SIAPIPE,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SIAPIPE,
	},
	[DB8500_REGULATOR_SWITCH_SGA] = {
		.desc = {
			.name	= "db8500-sga",
			.id	= DB8500_REGULATOR_SWITCH_SGA,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_SGA,
	},
	[DB8500_REGULATOR_SWITCH_B2R2_MCDE] = {
		.desc = {
			.name	= "db8500-b2r2-mcde",
			.id	= DB8500_REGULATOR_SWITCH_B2R2_MCDE,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_B2R2_MCDE,
	},
	[DB8500_REGULATOR_SWITCH_ESRAM12] = {
		.desc = {
			.name	= "db8500-esram12",
			.id	= DB8500_REGULATOR_SWITCH_ESRAM12,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id	= EPOD_ID_ESRAM12,
		.is_enabled	= true,
	},
	[DB8500_REGULATOR_SWITCH_ESRAM12RET] = {
		.desc = {
			.name	= "db8500-esram12-ret",
			.id	= DB8500_REGULATOR_SWITCH_ESRAM12RET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_ESRAM12,
		.is_ramret = true,
	},
	[DB8500_REGULATOR_SWITCH_ESRAM34] = {
		.desc = {
			.name	= "db8500-esram34",
			.id	= DB8500_REGULATOR_SWITCH_ESRAM34,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id	= EPOD_ID_ESRAM34,
		.is_enabled	= true,
	},
	[DB8500_REGULATOR_SWITCH_ESRAM34RET] = {
		.desc = {
			.name	= "db8500-esram34-ret",
			.id	= DB8500_REGULATOR_SWITCH_ESRAM34RET,
			.ops	= &ux500_regulator_switch_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
		.epod_id = EPOD_ID_ESRAM34,
		.is_ramret = true,
	},
};

static int __devinit db8500_regulator_probe(struct platform_device *pdev)
{
	int ret;

	ret = ux500_regulator_probe(pdev, db8500_regulator_info,
				    ARRAY_SIZE(db8500_regulator_info));
	if (!ret)
		regulator_has_full_constraints();

	return ret;
}

static int __devexit db8500_regulator_remove(struct platform_device *pdev)
{
	return ux500_regulator_remove(pdev, db8500_regulator_info,
				      ARRAY_SIZE(db8500_regulator_info));
}

static struct platform_driver db8500_regulator_driver = {
	.driver = {
		.name = "db8500-regulators",
		.owner = THIS_MODULE,
	},
	.remove = __devexit_p(db8500_regulator_remove),
};

static int __init db8500_regulator_init(void)
{
	int ret;

	ret = platform_driver_probe(&db8500_regulator_driver,
				    db8500_regulator_probe);
	if (ret < 0) {
		pr_info("db8500_regulator: platform_driver_register fails\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit db8500_regulator_exit(void)
{
	platform_driver_unregister(&db8500_regulator_driver);
}

/* replaced subsys_initcall as regulators must be turned on early */
arch_initcall(db8500_regulator_init);
module_exit(db8500_regulator_exit);

MODULE_AUTHOR("Bengt Jonsson <bengt.g.jonsson@stericsson.com>");
MODULE_DESCRIPTION("DB8500 regulator driver");
MODULE_LICENSE("GPL v2");
