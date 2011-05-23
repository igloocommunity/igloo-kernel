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

#include <mach/prcmu.h>

enum ape_opp_debug {
	APE_50_OPP_DEBUG,
	APE_100_OPP_DEBUG,
	NUM_APE_OPP_DEBUG,
};

enum ddr_opp_debug {
	DDR_25_OPP_DEBUG,
	DDR_50_OPP_DEBUG,
	DDR_100_OPP_DEBUG,
	NUM_DDR_OPP_DEBUG,
};

struct ape_state_history {
	ktime_t start;
	u32 state;
	u32 counter[NUM_APE_OPP_DEBUG];
	ktime_t time[NUM_APE_OPP_DEBUG];
	spinlock_t lock;
};

struct ddr_state_history {
	ktime_t start;
	u32 state;
	u32 counter[NUM_DDR_OPP_DEBUG];
	ktime_t time[NUM_DDR_OPP_DEBUG];
	spinlock_t lock;
};

static struct ape_state_history *ape_sh;
static struct ddr_state_history *ddr_sh;

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
	spin_lock_irqsave(&ape_sh->lock, flags);

	dtime = ktime_sub(now, ape_sh->start);
	ape_sh->time[state] = ktime_add(ape_sh->time[state], dtime);
	ape_sh->start = now;
	ape_sh->counter[state]++;
	ape_sh->state = state;

	spin_unlock_irqrestore(&ape_sh->lock, flags);
}

void prcmu_debug_ddr_opp_log(u8 opp)
{
	ktime_t now;
	ktime_t dtime;
	unsigned long flags;
	int state;

	if (opp == DDR_25_OPP)
		state = DDR_25_OPP_DEBUG;
	else if (opp == DDR_50_OPP)
		state = DDR_50_OPP_DEBUG;
	else
		state = DDR_100_OPP_DEBUG;

	now = ktime_get();
	spin_lock_irqsave(&ddr_sh->lock, flags);

	dtime = ktime_sub(now, ddr_sh->start);
	ddr_sh->time[state] = ktime_add(ddr_sh->time[state], dtime);
	ddr_sh->start = now;
	ddr_sh->counter[state]++;
	ddr_sh->state = state;

	spin_unlock_irqrestore(&ddr_sh->lock, flags);
}

static ssize_t ape_stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	unsigned long flags;
	int i;

	pr_info("/nreset\n");

	spin_lock_irqsave(&ape_sh->lock, flags);
	for (i = 0; i < NUM_APE_OPP_DEBUG; i++) {
		ape_sh->counter[i] = 0;
		ape_sh->time[i] = ktime_set(0, 0);
	}

	ape_sh->start = ktime_get();
	spin_unlock_irqrestore(&ape_sh->lock, flags);

	return count;
}

static ssize_t ddr_stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	unsigned long flags;
	int i;

	pr_info("/nreset\n");

	spin_lock_irqsave(&ddr_sh->lock, flags);
	for (i = 0; i < NUM_DDR_OPP_DEBUG; i++) {
		ddr_sh->counter[i] = 0;
		ddr_sh->time[i] = ktime_set(0, 0);
	}

	ddr_sh->start = ktime_get();
	spin_unlock_irqrestore(&ddr_sh->lock, flags);

	return count;
}

static int ape_stats_print(struct seq_file *s, void *p)
{
	int i;
	unsigned long flags;
	ktime_t total;
	ktime_t now;
	ktime_t dtime;
	s64 t_us;
	s64 perc;
	s64 total_us;

	spin_lock_irqsave(&ape_sh->lock, flags);
	/* Update time in state */
	now = ktime_get();
	dtime = ktime_sub(now, ape_sh->start);
	ape_sh->time[ape_sh->state] =
		ktime_add(ape_sh->time[ape_sh->state], dtime);
	ape_sh->start = now;

	/* Now print the stats */
	total = ktime_set(0, 0);

	for (i = 0; i < NUM_APE_OPP_DEBUG; i++)
		total = ktime_add(total, ape_sh->time[i]);
	total_us = ktime_to_us(total);
	do_div(total_us, 100);

	for (i = 0; i < NUM_APE_OPP_DEBUG; i++) {
		t_us = ktime_to_us(ape_sh->time[i]);
		perc = ktime_to_us(ape_sh->time[i]);
		do_div(t_us, 1000); /* to ms */
		do_div(perc, total_us);
		if (i == APE_50_OPP_DEBUG)
			seq_printf(s, "%s: # %u in %d ms %d%%\n",
				   "APE OPP 50% ",
				   ape_sh->counter[i],
				   (u32) t_us, (u32)perc);
		else
			seq_printf(s, "%s: # %u in %d ms %d%%\n",
				   "APE OPP 100%",
				   ape_sh->counter[i],
				   (u32) t_us, (u32)perc);

	}
	spin_unlock_irqrestore(&ape_sh->lock, flags);
	return 0;
}

static int ddr_stats_print(struct seq_file *s, void *p)
{
	int i;
	unsigned long flags;
	ktime_t total;
	ktime_t now;
	ktime_t dtime;
	s64 t_us;
	s64 perc;
	s64 total_us;

	spin_lock_irqsave(&ddr_sh->lock, flags);
	/* Update time in state */
	now = ktime_get();
	dtime = ktime_sub(now, ddr_sh->start);
	ddr_sh->time[ddr_sh->state] =
		ktime_add(ddr_sh->time[ddr_sh->state], dtime);
	ddr_sh->start = now;

	/* Now print the stats */
	total = ktime_set(0, 0);

	for (i = 0; i < NUM_DDR_OPP_DEBUG; i++)
		total = ktime_add(total, ddr_sh->time[i]);
	total_us = ktime_to_us(total);
	do_div(total_us, 100);

	for (i = 0; i < NUM_DDR_OPP_DEBUG; i++) {
		t_us = ktime_to_us(ddr_sh->time[i]);
		perc = ktime_to_us(ddr_sh->time[i]);
		do_div(t_us, 1000); /* to ms */
		do_div(perc, total_us);
		if (i == DDR_25_OPP_DEBUG)
			seq_printf(s, "%s: # %u in %d ms %d%%\n",
				   "DDR OPP 25% ",
				   ddr_sh->counter[i],
				   (u32) t_us, (u32)perc);
		else if (i == DDR_50_OPP_DEBUG)
			seq_printf(s, "%s: # %u in %d ms %d%%\n",
				   "DDR OPP 50% ",
				   ddr_sh->counter[i],
				   (u32) t_us, (u32)perc);
		else
			seq_printf(s, "%s: # %u in %d ms %d%%\n",
				   "DDR OPP 100%",
				   ddr_sh->counter[i],
				   (u32) t_us, (u32)perc);

	}
	spin_unlock_irqrestore(&ddr_sh->lock, flags);
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

static int ddr_opp_read(struct seq_file *s, void *p)
{
	int opp;

	opp = prcmu_get_ddr_opp();
	return seq_printf(s, "%s (%d)\n",
			(opp == DDR_100_OPP) ? "100%" :
			(opp == DDR_50_OPP) ? "50%" :
			(opp == DDR_25_OPP) ? "25%" :
			"unknown", opp);
}

static ssize_t opp_write(struct file *file,
				   const char __user *user_buf,
			 size_t count, loff_t *ppos, int prcmu_qos_class)
{
	char buf[32];
	ssize_t buf_size;
	long unsigned int i;

	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (strict_strtoul(buf, 0, &i) != 0)
		return buf_size;

	prcmu_qos_force_opp(prcmu_qos_class, i);

	pr_info("prcmu debug: forced OPP for %d to %d\n", prcmu_qos_class, (int)i);

	return buf_size;
}

static ssize_t ddr_opp_write(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	return opp_write(file, user_buf, count, ppos, PRCMU_QOS_DDR_OPP);
}

static ssize_t ape_opp_write(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	return opp_write(file, user_buf, count, ppos, PRCMU_QOS_APE_OPP);
}

static int cpufreq_delay_read(struct seq_file *s, void *p)
{
	return seq_printf(s, "%lu\n", prcmu_qos_get_cpufreq_opp_delay());
}

static ssize_t cpufreq_delay_write(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{

	char buf[32];
	ssize_t buf_size;
	long unsigned int i;

	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (strict_strtoul(buf, 0, &i) != 0)
		return buf_size;

	prcmu_qos_set_cpufreq_opp_delay(i);

	pr_info("prcmu debug: changed delay between cpufreq change and QoS "
		 "requirement to %lu.\n", i);

	return buf_size;
}

static int arm_opp_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, arm_opp_read, inode->i_private);
}

static int ape_opp_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ape_opp_read, inode->i_private);
}

static int ddr_opp_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ddr_opp_read, inode->i_private);
}

static int ape_stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ape_stats_print, inode->i_private);
}

static int ddr_stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, ddr_stats_print, inode->i_private);
}

static int cpufreq_delay_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, cpufreq_delay_read, inode->i_private);
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
	.write = ape_opp_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ddr_opp_fops = {
	.open = ddr_opp_open_file,
	.write = ddr_opp_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ape_stats_fops = {
	.open = ape_stats_open_file,
	.write = ape_stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ddr_stats_fops = {
	.open = ddr_stats_open_file,
	.write = ddr_stats_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations cpufreq_delay_fops = {
	.open = cpufreq_delay_open_file,
	.write = cpufreq_delay_write,
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
				   dir, NULL, &ape_stats_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ddr_stats", (S_IRUGO | S_IWUGO),
				   dir, NULL, &ddr_stats_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ape_opp", (S_IRUGO),
				   dir, NULL, &ape_opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("ddr_opp", (S_IRUGO),
				   dir, NULL, &ddr_opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("arm_opp", (S_IRUGO),
				   dir, NULL, &arm_opp_fops);
	if (IS_ERR_OR_NULL(file))
		goto fail;

	file = debugfs_create_file("opp_cpufreq_delay", (S_IRUGO),
				   dir, NULL, &cpufreq_delay_fops);
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
	ape_sh = kzalloc(sizeof(struct ape_state_history), GFP_KERNEL);
	if (ape_sh == NULL) {
		pr_err("prcmu debug: kzalloc failed\n");
		return -ENOMEM;
	}

	ddr_sh = kzalloc(sizeof(struct ddr_state_history), GFP_KERNEL);
	if (ddr_sh == NULL) {
		pr_err("prcmu debug: kzalloc failed\n");
		return -ENOMEM;
	}

	spin_lock_init(&ape_sh->lock);
	spin_lock_init(&ddr_sh->lock);
	ape_sh->start = ktime_get();
	ddr_sh->start = ktime_get();
	setup_debugfs();
	return 0;
}
