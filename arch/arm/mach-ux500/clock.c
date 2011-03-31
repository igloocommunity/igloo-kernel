/*
 *  Copyright (C) 2009 ST-Ericsson
 *  Copyright (C) 2009 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/mfd/ab8500/sysctrl.h>
#include <mach/prcmu-fw-api.h>

#include "clock.h"

#define PRCC_PCKEN 0x0
#define PRCC_PCKDIS 0x4
#define PRCC_KCKEN 0x8
#define PRCC_KCKDIS 0xC
#define PRCC_PCKSR 0x10
#define PRCC_KCKSR 0x14

DEFINE_MUTEX(clk_opp100_mutex);
static DEFINE_SPINLOCK(clk_spin_lock);
#define NO_LOCK &clk_spin_lock

static void __iomem *prcmu_base;

static void __clk_lock(struct clk *clk, void *last_lock, unsigned long *flags)
{
	if (clk->mutex != last_lock) {
		if (clk->mutex == NULL)
			spin_lock_irqsave(&clk_spin_lock, *flags);
		else
			mutex_lock(clk->mutex);
	}
}

static void __clk_unlock(struct clk *clk, void *last_lock, unsigned long flags)
{
	if (clk->mutex != last_lock) {
		if (clk->mutex == NULL)
			spin_unlock_irqrestore(&clk_spin_lock, flags);
		else
			mutex_unlock(clk->mutex);
	}
}

static void __clk_disable(struct clk *clk, void *current_lock)
{
	unsigned long flags;

	if (clk == NULL)
		return;

	__clk_lock(clk, current_lock, &flags);

	if (clk->enabled && (--clk->enabled == 0)) {
		if ((clk->ops != NULL) && (clk->ops->disable != NULL))
			clk->ops->disable(clk);
		__clk_disable(clk->parent, clk->mutex);
		__clk_disable(clk->bus_parent, clk->mutex);
	}

	__clk_unlock(clk, current_lock, flags);

	return;
}

static int __clk_enable(struct clk *clk, void *current_lock)
{
	int err;
	unsigned long flags;

	if (clk == NULL)
		return 0;

	__clk_lock(clk, current_lock, &flags);

	if (!clk->enabled) {
		err = __clk_enable(clk->bus_parent, clk->mutex);
		if (unlikely(err))
			goto bus_parent_error;

		err = __clk_enable(clk->parent, clk->mutex);
		if (unlikely(err))
			goto parent_error;

		if ((clk->ops != NULL) && (clk->ops->enable != NULL)) {
			err = clk->ops->enable(clk);
			if (unlikely(err))
				goto enable_error;
		}
	}
	clk->enabled++;

	__clk_unlock(clk, current_lock, flags);

	return 0;

enable_error:
	__clk_disable(clk->parent, clk->mutex);
parent_error:
	__clk_disable(clk->bus_parent, clk->mutex);
bus_parent_error:

	__clk_unlock(clk, current_lock, flags);

	return err;
}

unsigned long __clk_get_rate(struct clk *clk, void *current_lock)
{
	unsigned long rate;
	unsigned long flags;

	if (clk == NULL)
		return 0;

	__clk_lock(clk, current_lock, &flags);

	if ((clk->ops != NULL) && (clk->ops->get_rate != NULL))
		rate = clk->ops->get_rate(clk);
	else if (clk->rate)
		rate = clk->rate;
	else
		rate = __clk_get_rate(clk->parent, clk->mutex);

	__clk_unlock(clk, current_lock, flags);

	return rate;
}

static unsigned long __clk_round_rate(struct clk *clk, unsigned long rate)
{
	if ((clk->ops != NULL) && (clk->ops->round_rate != NULL))
		return clk->ops->round_rate(clk, rate);

	return -ENOSYS;
}

static int __clk_set_rate(struct clk *clk, unsigned long rate)
{
	if ((clk->ops != NULL) && (clk->ops->set_rate != NULL))
		return clk->ops->set_rate(clk, rate);

	return -ENOSYS;
}

int clk_enable(struct clk *clk)
{
	if (clk == NULL)
		return -EINVAL;

	return __clk_enable(clk, NO_LOCK);
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{

	if (clk == NULL)
		return;

	WARN_ON(!clk->enabled);
	__clk_disable(clk, NO_LOCK);
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	if (clk == NULL)
		return 0;

	return __clk_get_rate(clk, NO_LOCK);
}
EXPORT_SYMBOL(clk_get_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;

	if (clk == NULL)
		return -EINVAL;

	__clk_lock(clk, NO_LOCK, &flags);

	rate = __clk_round_rate(clk, rate);

	__clk_unlock(clk, NO_LOCK, flags);

	return rate;
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int err;
	unsigned long flags;

	if (clk == NULL)
		return -EINVAL;

	__clk_lock(clk, NO_LOCK, &flags);

	err =  __clk_set_rate(clk, rate);

	__clk_unlock(clk, NO_LOCK, flags);

	return err;
}
EXPORT_SYMBOL(clk_set_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int err = -EINVAL;
	unsigned long flags;
	struct clk **p;

	if ((clk == NULL) || (clk->parents == NULL))
		return -EINVAL;
	for (p = clk->parents; *p != parent; p++) {
		if (*p == NULL) /* invalid parent */
			return -EINVAL;
	}

	__clk_lock(clk, NO_LOCK, &flags);

	if (clk->enabled) {
		err = -EINVAL;
	} else {
		if ((clk->ops != NULL) && (clk->ops->set_parent != NULL))
			err = clk->ops->set_parent(clk, parent);
		if (!err)
			clk->parent = parent;
	}

	__clk_unlock(clk, NO_LOCK, flags);

	return err;
}

/* PRCMU clock operations. */

static int prcmu_clk_enable(struct clk *clk)
{
	return prcmu_request_clock(clk->cg_sel, true);
}

static void prcmu_clk_disable(struct clk *clk)
{
	if (prcmu_request_clock(clk->cg_sel, false)) {
		pr_err("clock: %s failed to disable %s.\n", __func__,
			clk->name);
	}
}

static int request_ape_opp100(bool enable)
{
	static unsigned int requests;

	if (enable) {
	       if (0 == requests++) {
		       return prcmu_qos_add_requirement(PRCMU_QOS_APE_OPP,
			       "clock", 100);
	       }
	} else if (1 == requests--) {
		prcmu_qos_remove_requirement(PRCMU_QOS_APE_OPP, "clock");
	}
	return 0;
}

static int prcmu_opp100_clk_enable(struct clk *clk)
{
	int r;

	r = request_ape_opp100(true);
	if (r) {
		pr_err("clock: %s failed to request APE OPP 100%% for %s.\n",
			__func__, clk->name);
		return r;
	}
	return prcmu_request_clock(clk->cg_sel, true);
}

static void prcmu_opp100_clk_disable(struct clk *clk)
{
	if (prcmu_request_clock(clk->cg_sel, false))
		goto out_error;
	if (request_ape_opp100(false))
		goto out_error;
	return;

out_error:
	pr_err("clock: %s failed to disable %s.\n", __func__, clk->name);
}

struct clkops prcmu_clk_ops = {
	.enable = prcmu_clk_enable,
	.disable = prcmu_clk_disable,
};

struct clkops prcmu_opp100_clk_ops = {
	.enable = prcmu_opp100_clk_enable,
	.disable = prcmu_opp100_clk_disable,
};

/* PRCC clock operations. */

static int prcc_pclk_enable(struct clk *clk)
{
	void __iomem *io_base = __io_address(clk->io_base);

	writel(clk->cg_sel, (io_base + PRCC_PCKEN));
	while (!(readl(io_base + PRCC_PCKSR) & clk->cg_sel))
		cpu_relax();
	return 0;
}

static void prcc_pclk_disable(struct clk *clk)
{
	void __iomem *io_base = __io_address(clk->io_base);

	writel(clk->cg_sel, (io_base + PRCC_PCKDIS));
}

struct clkops prcc_pclk_ops = {
	.enable = prcc_pclk_enable,
	.disable = prcc_pclk_disable,
};

static int prcc_kclk_enable(struct clk *clk)
{
	void __iomem *io_base = __io_address(clk->io_base);

	writel(clk->cg_sel, (io_base + PRCC_KCKEN));
	while (!(readl(io_base + PRCC_KCKSR) & clk->cg_sel))
		cpu_relax();
	return 0;
}

static void prcc_kclk_disable(struct clk *clk)
{
	void __iomem *io_base = __io_address(clk->io_base);

	writel(clk->cg_sel, (io_base + PRCC_KCKDIS));
}

struct clkops prcc_kclk_ops = {
	.enable = prcc_kclk_enable,
	.disable = prcc_kclk_disable,
};

void clks_register(struct clk_lookup *clks, size_t num)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		clkdev_add(&clks[i]);
}

int __init clk_init(void)
{
	if (cpu_is_u8500()) {
		prcmu_base = __io_address(U8500_PRCMU_BASE);
	} else if (cpu_is_u5500()) {
		prcmu_base = __io_address(U5500_PRCMU_BASE);
	} else {
		pr_err("clock: Unknown DB Asic.\n");
		return -EIO;
	}

	if (cpu_is_u8500())
		db8500_clk_init();
	/* Here the DB5500 clock tree will be added */

	return 0;
}
