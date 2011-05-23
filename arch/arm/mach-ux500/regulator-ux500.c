/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Authors: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 *          Bengt Jonsson <bengt.g.jonsson@stericsson.com> for ST-Ericsson
 *
 * Power domain regulators on UX500
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include "regulator-ux500.h"

#include <mach/prcmu.h>

/*
 * power state reference count
 */
static int power_state_active_cnt; /* will initialize to zero */
static DEFINE_SPINLOCK(power_state_active_lock);

static void power_state_active_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&power_state_active_lock, flags);
	power_state_active_cnt++;
	spin_unlock_irqrestore(&power_state_active_lock, flags);
}

static int power_state_active_disable(void)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&power_state_active_lock, flags);
	if (power_state_active_cnt <= 0) {
		pr_err("power state: unbalanced enable/disable calls\n");
		ret = -EINVAL;
		goto out;
	}

	power_state_active_cnt--;
out:
	spin_unlock_irqrestore(&power_state_active_lock, flags);
	return ret;
}

/*
 * Exported interface for CPUIdle only. This function is called when interrupts
 * are turned off. Hence, no locking.
 */
int power_state_active_is_enabled(void)
{
	return (power_state_active_cnt > 0);
}

struct ux500_regulator {
	char *name;
	void (*enable)(void);
	int (*disable)(void);
};

/*
 * Don't add any clients to this struct without checking with regulator
 * responsible!
 */
static struct ux500_regulator ux500_atomic_regulators[] = {
	{
		.name    = "dma40.0",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "ssp0",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "ssp1",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "spi0",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "spi1",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "spi2",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "spi3",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "cryp1",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
	{
		.name = "hash1",
		.enable  = power_state_active_enable,
		.disable = power_state_active_disable,
	},
};

struct ux500_regulator *__must_check ux500_regulator_get(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ux500_atomic_regulators); i++) {
		if (!strcmp(dev_name(dev), ux500_atomic_regulators[i].name))
			return &ux500_atomic_regulators[i];
	}

	return  ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL_GPL(ux500_regulator_get);

int ux500_regulator_atomic_enable(struct ux500_regulator *regulator)
{
	if (regulator) {
		regulator->enable();
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(ux500_regulator_atomic_enable);

int ux500_regulator_atomic_disable(struct ux500_regulator *regulator)
{
	if (regulator)
		return regulator->disable();
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(ux500_regulator_atomic_disable);

void ux500_regulator_put(struct ux500_regulator *regulator)
{
	/* Here for symetric reasons and for possible future use */
}
EXPORT_SYMBOL_GPL(ux500_regulator_put);

static int u8500_regulator_enable(struct regulator_dev *rdev)
{
	struct u8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL)
		return -EINVAL;

	dev_vdbg(rdev_get_dev(rdev), "regulator-%s-enable\n",
		info->desc.name);

	info->is_enabled = true;
	if (!info->exclude_from_power_state)
		power_state_active_enable();

	return 0;
}

static int u8500_regulator_disable(struct regulator_dev *rdev)
{
	struct u8500_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;

	if (info == NULL)
		return -EINVAL;

	dev_vdbg(rdev_get_dev(rdev), "regulator-%s-disable\n",
		info->desc.name);

	info->is_enabled = false;
	if (!info->exclude_from_power_state)
		ret = power_state_active_disable();

	return ret;
}

static int u8500_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct u8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL)
		return -EINVAL;

	dev_vdbg(rdev_get_dev(rdev), "regulator-%s-is_enabled (is_enabled):"
		" %i\n", info->desc.name, info->is_enabled);

	return info->is_enabled;
}

/* u8500 regulator operations */
struct regulator_ops ux500_regulator_ops = {
	.enable			= u8500_regulator_enable,
	.disable		= u8500_regulator_disable,
	.is_enabled		= u8500_regulator_is_enabled,
};

/*
 * EPOD control
 */
static bool epod_on[NUM_EPOD_ID];
static bool epod_ramret[NUM_EPOD_ID];

static int enable_epod(u16 epod_id, bool ramret)
{
	int ret;

	if (ramret) {
		if (!epod_on[epod_id]) {
			ret = prcmu_set_epod(epod_id, EPOD_STATE_RAMRET);
			if (ret < 0)
				return ret;
		}
		epod_ramret[epod_id] = true;
	} else {
		ret = prcmu_set_epod(epod_id, EPOD_STATE_ON);
		if (ret < 0)
			return ret;
		epod_on[epod_id] = true;
	}

	return 0;
}

static int disable_epod(u16 epod_id, bool ramret)
{
	int ret;

	if (ramret) {
		if (!epod_on[epod_id]) {
			ret = prcmu_set_epod(epod_id, EPOD_STATE_OFF);
			if (ret < 0)
				return ret;
		}
		epod_ramret[epod_id] = false;
	} else {
		if (epod_ramret[epod_id]) {
			ret = prcmu_set_epod(epod_id, EPOD_STATE_RAMRET);
			if (ret < 0)
				return ret;
		} else {
			ret = prcmu_set_epod(epod_id, EPOD_STATE_OFF);
			if (ret < 0)
				return ret;
		}
		epod_on[epod_id] = false;
	}

	return 0;
}

/*
 * Regulator switch
 */
static int u8500_regulator_switch_enable(struct regulator_dev *rdev)
{
	struct u8500_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;

	if (info == NULL)
		return -EINVAL;

	dev_vdbg(rdev_get_dev(rdev), "regulator-switch-%s-enable\n",
		info->desc.name);

	ret = enable_epod(info->epod_id, info->is_ramret);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev),
			"regulator-switch-%s-enable: prcmu call failed\n",
			info->desc.name);
		goto out;
	}

	info->is_enabled = true;
out:
	return ret;
}

static int u8500_regulator_switch_disable(struct regulator_dev *rdev)
{
	struct u8500_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;

	if (info == NULL)
		return -EINVAL;

	dev_vdbg(rdev_get_dev(rdev), "regulator-switch-%s-disable\n",
		info->desc.name);

	ret = disable_epod(info->epod_id, info->is_ramret);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev),
			"regulator_switch-%s-disable: prcmu call failed\n",
			info->desc.name);
		goto out;
	}

	info->is_enabled = 0;
out:
	return ret;
}

static int u8500_regulator_switch_is_enabled(struct regulator_dev *rdev)
{
	struct u8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL)
		return -EINVAL;

	dev_vdbg(rdev_get_dev(rdev),
		"regulator-switch-%s-is_enabled (is_enabled): %i\n",
		info->desc.name, info->is_enabled);

	return info->is_enabled;
}

struct regulator_ops ux500_regulator_switch_ops = {
	.enable			= u8500_regulator_switch_enable,
	.disable		= u8500_regulator_switch_disable,
	.is_enabled		= u8500_regulator_switch_is_enabled,
};

int __devinit
ux500_regulator_probe(struct platform_device *pdev,
		      struct u8500_regulator_info *regulator_info,
		      int num_regulators)
{
	struct regulator_init_data *u8500_init_data =
		dev_get_platdata(&pdev->dev);
	int i, err;

	/* register all regulators */
	for (i = 0; i < num_regulators; i++) {
		struct u8500_regulator_info *info;
		struct regulator_init_data *init_data = &u8500_init_data[i];

		/* assign per-regulator data */
		info = &regulator_info[i];
		info->dev = &pdev->dev;

		/* register with the regulator framework */
		info->rdev = regulator_register(&info->desc, &pdev->dev,
				init_data, info);
		if (IS_ERR(info->rdev)) {
			err = PTR_ERR(info->rdev);
			dev_err(&pdev->dev, "failed to register %s: err %i\n",
				info->desc.name, err);

			/* if failing, unregister all earlier regulators */
			i--;
			while (i >= 0) {
				info = &regulator_info[i];
				regulator_unregister(info->rdev);
				i--;
			}
			return err;
		}

		dev_vdbg(rdev_get_dev(info->rdev),
			"regulator-%s-probed\n", info->desc.name);
	}

	return 0;
}

int __devexit
ux500_regulator_remove(struct platform_device *pdev,
		       struct u8500_regulator_info *regulator_info,
		       int num_regulators)
{
	int i;

	for (i = 0; i < num_regulators; i++) {
		struct u8500_regulator_info *info;

		info = &regulator_info[i];

		dev_vdbg(rdev_get_dev(info->rdev),
			"regulator-%s-remove\n", info->desc.name);

		regulator_unregister(info->rdev);
	}

	return 0;
}
