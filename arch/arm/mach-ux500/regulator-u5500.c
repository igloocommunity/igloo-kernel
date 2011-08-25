/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/prcmu.h>

#include "regulator-ux500.h"
#include "regulator-u5500.h"

#define U5500_REGULATOR_SWITCH(_name, reg)                               \
	[U5500_REGULATOR_SWITCH_##reg] = {                               \
		.desc = {                                               \
			.name   = _name,                                \
			.id     = U5500_REGULATOR_SWITCH_##reg,          \
			.ops    = &ux500_regulator_switch_ops,          \
			.type   = REGULATOR_VOLTAGE,                    \
			.owner  = THIS_MODULE,                          \
		},                                                      \
		.epod_id = DB5500_EPOD_ID_##reg,                         \
}

static struct u8500_regulator_info
u5500_regulator_info[U5500_NUM_REGULATORS] = {
	[U5500_REGULATOR_VAPE] = {
		.desc = {
			.name	= "u5500-vape",
			.id	= U5500_REGULATOR_VAPE,
			.ops	= &ux500_regulator_ops,
			.type	= REGULATOR_VOLTAGE,
			.owner	= THIS_MODULE,
		},
	},
	U5500_REGULATOR_SWITCH("u5500-sga", SGA),
	U5500_REGULATOR_SWITCH("u5500-hva", HVA),
	U5500_REGULATOR_SWITCH("u5500-sia", SIA),
	U5500_REGULATOR_SWITCH("u5500-disp", DISP),
	U5500_REGULATOR_SWITCH("u5500-esram12", ESRAM12),
};

static int __devinit u5500_regulator_probe(struct platform_device *pdev)
{
	return ux500_regulator_probe(pdev, u5500_regulator_info,
				     ARRAY_SIZE(u5500_regulator_info));
}

static int __devexit u5500_regulator_remove(struct platform_device *pdev)
{
	return ux500_regulator_remove(pdev, u5500_regulator_info,
				      ARRAY_SIZE(u5500_regulator_info));
}

static struct platform_driver u5500_regulator_driver = {
	.driver = {
		.name = "u5500-regulators",
		.owner = THIS_MODULE,
	},
	.probe	= u5500_regulator_probe,
	.remove = __devexit_p(u5500_regulator_remove),
};

static int __init u5500_regulator_init(void)
{
	return platform_driver_register(&u5500_regulator_driver);
}

static void __exit u5500_regulator_exit(void)
{
	platform_driver_unregister(&u5500_regulator_driver);
}

arch_initcall(u5500_regulator_init);
module_exit(u5500_regulator_exit);

MODULE_DESCRIPTION("U5500 regulator driver");
MODULE_LICENSE("GPL v2");
