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
#ifdef CONFIG_DEBUG_FS
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>	/* for copy_from_user */
#include <linux/kernel.h>
#endif

#include <mach/prcmu-fw-api.h>

#include "clock.h"

#ifdef CONFIG_DEBUG_FS
static LIST_HEAD(clk_list);
#endif

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

static struct clk *__clk_get_parent(struct clk *clk)
{
	if (clk->parent != NULL)
		return clk->parent;
	else if ((clk->bus_parent != NULL))
		return clk->bus_parent;

	return NULL;
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

struct clk *clk_get_parent(struct clk *clk)
{
	if (clk == NULL)
		return ERR_PTR(-EINVAL);

	return __clk_get_parent(clk);
}
EXPORT_SYMBOL(clk_get_parent);

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
	int err = 0;
	unsigned long flags;
	struct clk **p;

	if ((clk == NULL) || (clk->parents == NULL))
		return -EINVAL;
	for (p = clk->parents; *p != parent; p++) {
		if (*p == NULL) /* invalid parent */
			return -EINVAL;
	}

	__clk_lock(clk, NO_LOCK, &flags);

	if ((clk->ops != NULL) && (clk->ops->set_parent != NULL)) {
		err = clk->ops->set_parent(clk, parent);
		if (err)
			goto unlock_and_return;
	} else if (clk->enabled) {
		err = __clk_enable(parent, clk->mutex);
		if (err)
			goto unlock_and_return;
		__clk_disable(clk->parent, clk->mutex);
	}

	clk->parent = parent;

unlock_and_return:
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

	for (i = 0; i < num; i++) {
		clkdev_add(&clks[i]);
#ifdef CONFIG_DEBUG_FS
		/* Check that the clock has not been already registered */
		if (!(clks[i].clk->list.prev != clks[i].clk->list.next))
			list_add_tail(&clks[i].clk->list, &clk_list);
#endif
	}
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
	else if (cpu_is_u5500())
		db5500_clk_init();

	return 0;
}

#ifdef CONFIG_DEBUG_FS
/*
 *	debugfs support to trace clock tree hierarchy and attributes with
 *	powerdebug
 */
static struct dentry *clk_debugfs_root;

#ifdef CONFIG_DEBUG_FS_WRITE
static ssize_t enable_dbg_write(struct file *file, const char __user *buf,
						 size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;

	clk_enable(clk);
	return size;
}

static ssize_t disable_dbg_write(struct file *file, const char __user *buf,
						  size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;

	clk_disable(clk);
	return size;
}

static ssize_t rate_dbg_write(struct file *file, const char __user *buf,
					   size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;
	char crate[128];
	unsigned long rate;

	if (size < (sizeof(crate)-1)) {
		if (copy_from_user(crate, buf, size))
			return -EFAULT;
		crate[size] = 0;
		if (!strict_strtoul(crate, 10, &rate))
			clk_set_rate(clk, rate);
		return size;
	}
	return -EINVAL;
}

static ssize_t parent_dbg_write(struct file *file, const char __user *buf,
						 size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;
	char cname[128];
	struct clk *pclk;

	if (size < (sizeof(cname) - 1)) {
		if (copy_from_user(cname, buf, size))
			return -EFAULT;
		cname[size] = 0;
		pclk = clk_get_sys(cname, cname);
		clk_set_parent(clk, pclk);
		return size;
	}
	return -EINVAL;
}
#endif

static ssize_t usecount_dbg_read(struct file *file, char __user *buf,
						  size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;
	char cusecount[128];
	unsigned int len;

	len = sprintf(cusecount, "%u\n", clk->enabled);
	return simple_read_from_buffer(buf, size, off, cusecount, len);
}

static ssize_t rate_dbg_read(struct file *file, char __user *buf,
					  size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;
	char crate[128];
	unsigned int rate;
	unsigned int len;

	rate = clk_get_rate(clk);
	len = sprintf(crate, "%u\n", rate);
	return simple_read_from_buffer(buf, size, off, crate, len);
}

static ssize_t parent_dbg_read(struct file *file, char __user *buf,
						size_t size, loff_t *off)
{
	struct clk *clk = file->f_dentry->d_inode->i_private;
	char cname[128];
	struct clk *pclk;
	unsigned int len;

	pclk = clk_get_parent(clk);
	if (pclk)
		len = sprintf(cname, "%s\n", pclk->name);
	else
		len = sprintf(cname, "No parent\n");
	return simple_read_from_buffer(buf, size, off, cname, len);
}

#ifdef CONFIG_DEBUG_FS_WRITE
static const struct file_operations enable_fops = {
	.write = enable_dbg_write,
};

static const struct file_operations disable_fops = {
	.write = disable_dbg_write,
};
#endif

static const struct file_operations usecount_fops = {
	.read = usecount_dbg_read,
};

static const struct file_operations set_rate_fops = {
#ifdef CONFIG_DEBUG_FS_WRITE
	.write = rate_dbg_write,
#endif
	.read = rate_dbg_read,
};

static const struct file_operations set_parent_fops = {
#ifdef CONFIG_DEBUG_FS_WRITE
	.write = parent_dbg_write,
#endif
	.read = parent_dbg_read,
};

static struct dentry *clk_debugfs_register_dir(struct clk *c,
						struct dentry *p_dentry)
{
	struct dentry *d, *clk_d, *child, *child_tmp;
	char s[255];
	char *p = s;

	if (c->name == NULL)
		p += sprintf(p, "BUG");
	else
		p += sprintf(p, "%s", c->name);

	clk_d = debugfs_create_dir(s, p_dentry);
	if (!clk_d)
		return NULL;

	d = debugfs_create_file("usecount", S_IRUGO,
			clk_d, c, &usecount_fops);
	if (!d)
		goto err_out;
#ifdef CONFIG_DEBUG_FS_WRITE
	d = debugfs_create_file("enable", S_IWUGO,
			clk_d, c, &enable_fops);
	if (!d)
		goto err_out;
	d = debugfs_create_file("disable", S_IWUGO,
			clk_d, c, &disable_fops);
	if (!d)
		goto err_out;
	d = debugfs_create_file("rate", S_IRUGO | S_IWUGO,
			clk_d, c, &set_rate_fops);
	if (!d)
		goto err_out;
	d = debugfs_create_file("parent",  S_IRUGO | S_IWUGO,
			clk_d, c, &set_parent_fops);
	if (!d)
		goto err_out;
#else
	d = debugfs_create_file("rate", S_IRUGO,
			clk_d, c, &set_rate_fops);
	if (!d)
		goto err_out;
	d = debugfs_create_file("parent",  S_IRUGO,
			clk_d, c, &set_parent_fops);
	if (!d)
		goto err_out;
#endif
	/*
	 * TODO : not currently available in ux500
	 * d = debugfs_create_x32("flags", S_IRUGO, clk_d, (u32 *)&c->flags);
	 * if (!d)
	 *	goto err_out;
	 */

	return clk_d;

err_out:
	d = clk_d;
	list_for_each_entry_safe(child, child_tmp, &d->d_subdirs, d_u.d_child)
		debugfs_remove(child);
	debugfs_remove(clk_d);
	return NULL;
}

static void clk_debugfs_remove_dir(struct dentry *cdentry)
{
	struct dentry *d, *child, *child_tmp;

	d = cdentry;
	list_for_each_entry_safe(child, child_tmp, &d->d_subdirs, d_u.d_child)
		debugfs_remove(child);
	debugfs_remove(cdentry);
	return ;
}


static int clk_debugfs_register_one(struct clk *c)
{
	struct clk *pa = c->parent;
	struct clk *bpa = c->bus_parent;

	if (!(bpa && !pa)) {
		c->dent = clk_debugfs_register_dir(c,
				pa ? pa->dent : clk_debugfs_root);
		if (!c->dent)
			return -ENOMEM;
	}

	if (bpa) {
		c->dent_bus = clk_debugfs_register_dir(c,
				bpa->dent_bus ? bpa->dent_bus : bpa->dent);
		if ((!c->dent_bus) &&  (c->dent)) {
			clk_debugfs_remove_dir(c->dent);
			c->dent = NULL;
			return -ENOMEM;
		}
	}
	return 0;
}

static int clk_debugfs_register(struct clk *c)
{
	int err;
	struct clk *pa = c->parent;
	struct clk *bpa = c->bus_parent;

	if (pa && (!pa->dent && !pa->dent_bus)) {
		err = clk_debugfs_register(pa);
		if (err)
			return err;
	}

	if (bpa && (!bpa->dent && !bpa->dent_bus)) {
		err = clk_debugfs_register(bpa);
		if (err)
			return err;
	}

	if ((!c->dent) && (!c->dent_bus)) {
		err = clk_debugfs_register_one(c);
		if (err)
			return err;
	}
	return 0;
}

static int __init clk_debugfs_init(void)
{
	struct clk *c;
	struct dentry *d;
	int err;

	d = debugfs_create_dir("clock", NULL);
	if (!d)
		return -ENOMEM;
	clk_debugfs_root = d;

	list_for_each_entry(c, &clk_list, list) {
		err = clk_debugfs_register(c);
		if (err)
			goto err_out;
	}
	return 0;
err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return err;
}

late_initcall(clk_debugfs_init);
#endif /* defined(CONFIG_DEBUG_FS) */


