/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Martin Persson <martin.persson@stericsson.com> for ST-Ericsson
 *         Etienne Carriere <etienne.carriere@stericsson.com> for ST-Ericsson
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <mach/prcmu-fw-api.h>

enum ape_opp_debug {
	APE_50_OPP_DEBUG,
	APE_100_OPP_DEBUG,
	NUM_APE_OPP_DEBUG,
};

struct state_history {
	ktime_t start;
	u32 state;
	u32 counter[NUM_APE_OPP_DEBUG];
	ktime_t time[NUM_APE_OPP_DEBUG];
	spinlock_t lock;
};

static struct state_history *sh;

void prcmu_debug_ape_opp_log(u8 opp)
{
	ktime_t now;
	ktime_t dtime;
	unsigned long flags;
	int state;

	if (opp == APE_50_OPP)
		state = APE_50_OPP_DEBUG;
	else
		state = APE_100_OPP_DEBUG;

	now = ktime_get();
	spin_lock_irqsave(&sh->lock, flags);

	dtime = ktime_sub(now, sh->start);
	sh->time[state] = ktime_add(sh->time[state], dtime);
	sh->start = now;
	sh->counter[state]++;
	sh->state = state;

	spin_unlock_irqrestore(&sh->lock, flags);
}

static ssize_t stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	unsigned long flags;
	int i;

	pr_info("/nreset\n");

	spin_lock_irqsave(&sh->lock, flags);
	for (i = 0; i < NUM_APE_OPP_DEBUG; i++) {
		sh->counter[i] = 0;
		sh->time[i] = ktime_set(0, 0);
	}

	sh->start = ktime_get();
	spin_unlock_irqrestore(&sh->lock, flags);

	return count;
}

static int stats_print(struct seq_file *s, void *p)
{
	int i;
	unsigned long flags;
	ktime_t total;
	ktime_t now;
	ktime_t dtime;
	s64 t_us;
	s64 perc;
	s64 total_us;

	spin_lock_irqsave(&sh->lock, flags);
	/* Update time in state */
	now = ktime_get();
	dtime = ktime_sub(now, sh->start);
	sh->time[sh->state] = ktime_add(sh->time[sh->state], dtime);
	sh->start = now;

	/* Now print the stats */
	total = ktime_set(0, 0);

	for (i = 0; i < NUM_APE_OPP_DEBUG; i++)
		total = ktime_add(total, sh->time[i]);
	total_us = ktime_to_us(total);
	do_div(total_us, 100);

	for (i = 0; i < NUM_APE_OPP_DEBUG; i++) {
		t_us = ktime_to_us(sh->time[i]);
		perc = ktime_to_us(sh->time[i]);
		do_div(t_us, 1000); /* to ms */
		do_div(perc, total_us);
		if (i == APE_50_OPP_DEBUG)
			seq_printf(s, "%s: # %u in %d ms %d\n",
				   "APE OPP 50% ",
				   sh->counter[i],
				   (u32) t_us, (u32)perc);
		else
			seq_printf(s, "%s: # %u in %d ms %d\n",
				   "APE OPP 100%",
				   sh->counter[i],
				   (u32) t_us, (u32)perc);

	}
	spin_unlock_irqrestore(&sh->lock, flags);
	return 0;
}

static int arm_opp_read(struct seq_file *s, void *p)
{
	int opp;

	opp = prcmu_get_arm_opp();
	return seq_printf(s, "%s (%d)\n",
		(opp == ARM_MAX_OPP) ? "max" :
		(opp == ARM_MAX_FREQ100OPP) ? "max-freq100" :
		(opp == ARM_100_OPP) ? "100%" :
		(opp == ARM_50_OPP) ? "50%" :
		(opp == ARM_EXTCLK) ? "25% (extclk)" :
		"unknown", opp);
}

static int ape_opp_read(struct seq_file *s, void *p)
{
	int opp;

	opp = prcmu_get_ape_opp();
	return seq_printf(s, "%s (%d)\n",
			(opp == APE_100_OPP) ? "100%" :
			(opp == APE_50_OPP) ? "50%" :
			"unknown", opp);
}

static int arm_opp_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, arm_opp_read, inode->i_private);
}

static int ape_opp_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ape_opp_read, inode->i_private);
}

static int stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, stats_print, inode->i_private);
}

static const struct file_operations arm_opp_fops = {
	.open = arm_opp_open_file,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ape_opp_fops = {
	.open = ape_opp_open_file,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations stats_fops = {
	.open = stats_open_file,
	.write = stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int setup_debugfs(void)
{
	struct dentry *dir;
	struct dentry *file;

	dir = debugfs_create_dir("prcmu", NULL);
	if (IS_ERR_OR_NULL(dir))
		goto fail;

	file = debugfs_create_file("ape_stats", (S_IRUGO | S_IWUGO),
				   dir, NULL, &stats_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ape_opp", (S_IRUGO),
				   dir, NULL, &ape_opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("arm_opp", (S_IRUGO),
				   dir, NULL, &arm_opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	return 0;
fail:
	if ((file == NULL) && (dir != NULL))
		debugfs_remove_recursive(dir);

	pr_err("prcmu debug: debugfs entry failed\n");
	return -ENOMEM;
}

int prcmu_debug_init(void)
{
	sh = kzalloc(sizeof(struct state_history), GFP_KERNEL);
	if (sh < 0) {
		pr_err("prcmu debug: kzalloc failed\n");
		return -ENOMEM;
	}

	spin_lock_init(&sh->lock);
	sh->start = ktime_get();
	setup_debugfs();
	return 0;
}
