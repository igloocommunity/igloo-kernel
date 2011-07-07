/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Authors: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 *          Bengt Jonsson <bengt.g.jonsson@stericsson.com> for ST-Ericsson
 *
 * UX500 common part of Power domain regulators (atomic regulators)
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/dbx500-prcmu.h>

/*
 * power state reference count
 */
static int power_state_active_cnt; /* will initialize to zero */
static DEFINE_SPINLOCK(power_state_active_lock);

void power_state_active_enable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&power_state_active_lock, flags);
	power_state_active_cnt++;
	spin_unlock_irqrestore(&power_state_active_lock, flags);
}
EXPORT_SYMBOL_GPL(power_state_active_enable);

int power_state_active_disable(void)
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
EXPORT_SYMBOL_GPL(power_state_active_disable);

/*
 * Exported interface for CPUIdle only. This function is called when interrupts
 * are turned off. Hence, no locking.
 */
int power_state_active_is_enabled(void)
{
	return (power_state_active_cnt > 0);
}
EXPORT_SYMBOL_GPL(power_state_active_is_enabled);

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
