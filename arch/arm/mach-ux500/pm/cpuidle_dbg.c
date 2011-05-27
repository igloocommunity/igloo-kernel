/*
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 */

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/amba/serial.h>

#include <linux/gpio.h>
#include <asm/hardware/gic.h>

#include "cpuidle.h"
#include "pm.h"
#include "timer.h"

#define APE_ON_TIMER_INTERVAL 5 /* Seconds */

#define UART_RX_GPIO_PIN_MASK (1 << (CONFIG_UX500_CONSOLE_UART_GPIO_PIN % 32))

#define UART011_MIS_RTIS (1 << 6) /* receive timeout interrupt status */
#define UART011_MIS_RXIS (1 << 4) /* receive interrupt status */
#define UART011_MIS 0x40 /* Masked interrupt status register */

enum latency_type {
	LATENCY_ENTER = 0,
	LATENCY_EXIT,
	LATENCY_WAKE,
	NUM_LATENCY,
};

struct state_history_state {
	u32 counter;
	ktime_t time;
	u32 hit_rate;
	u32 state_ok;
	u32 state_error;
	u32 state_int;

	u32 latency_count[NUM_LATENCY];
	ktime_t latency_sum[NUM_LATENCY];
	ktime_t latency_min[NUM_LATENCY];
	ktime_t latency_max[NUM_LATENCY];
};

struct state_history {
	ktime_t start;
	u32 state;
	u32 exit_counter;
	u32 timed_out;
	ktime_t measure_begin;
	int ape_blocked;
	int time_blocked;
	int both_blocked;
	int gov_blocked;
	struct state_history_state *states;
};
static DEFINE_PER_CPU(struct state_history, *state_history);

static struct delayed_work cpuidle_work;
static u32 dbg_console_enable = 1;
static void __iomem *uart_base;
static struct clk *uart_clk;

/* Blocks ApSleep and ApDeepSleep */
static bool force_APE_on;
static bool reset_timer;
static int deepest_allowed_state = CONFIG_U8500_CPUIDLE_DEEPEST_STATE;
static u32 measure_latency;
static bool wake_latency;

static bool apidle_both_blocked;
static bool apidle_ape_blocked;
static bool apidle_time_blocked;
static bool apidle_gov_blocked;

static struct cstate *cstates;
static int cstates_len;
static DEFINE_SPINLOCK(dbg_lock);

bool ux500_ci_dbg_force_ape_on(void)
{
	clk_enable(uart_clk);
	if (readw(uart_base + UART01x_FR) & UART01x_FR_BUSY) {
		clk_disable(uart_clk);
		return true;
	}
	clk_disable(uart_clk);

	return force_APE_on;
}

int ux500_ci_dbg_deepest_state(void)
{
	return deepest_allowed_state;
}

void ux500_ci_dbg_console_handle_ape_suspend(void)
{
	if (!dbg_console_enable)
		return;

	set_irq_wake(NOMADIK_GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 1);
	set_irq_type(NOMADIK_GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN),
		     IRQ_TYPE_EDGE_BOTH);
}

void ux500_ci_dbg_console_handle_ape_resume(void)
{
	unsigned long flags;
	u32 WKS_reg_value;

	if (!dbg_console_enable)
		return;

	WKS_reg_value = ux500_pm_gpio_read_wake_up_status(0);

	if (WKS_reg_value & UART_RX_GPIO_PIN_MASK) {
		spin_lock_irqsave(&dbg_lock, flags);
		reset_timer = true;
		spin_unlock_irqrestore(&dbg_lock, flags);
	}
	set_irq_wake(NOMADIK_GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 0);

}

void ux500_ci_dbg_console_check_uart(void)
{
	unsigned long flags;
	u32 status;

	if (!dbg_console_enable)
		return;

	clk_enable(uart_clk);
	spin_lock_irqsave(&dbg_lock, flags);
	status = readw(uart_base + UART011_MIS);

	if (status & (UART011_MIS_RTIS | UART011_MIS_RXIS))
		reset_timer = true;

	spin_unlock_irqrestore(&dbg_lock, flags);
	clk_disable(uart_clk);
}

void ux500_ci_dbg_console(void)
{
	unsigned long flags;

	if (!dbg_console_enable)
		return;

	spin_lock_irqsave(&dbg_lock, flags);
	if (reset_timer) {
		reset_timer = false;
		spin_unlock_irqrestore(&dbg_lock, flags);

		cancel_delayed_work(&cpuidle_work);
		force_APE_on = true;
		schedule_delayed_work(&cpuidle_work,
				      msecs_to_jiffies(APE_ON_TIMER_INTERVAL *
						       1000));
	} else {
		spin_unlock_irqrestore(&dbg_lock, flags);
	}
}

static void dbg_cpuidle_work_function(struct work_struct *work)
{
	force_APE_on = false;
}

static void store_latency(struct state_history *sh,
			  int ctarget,
			  enum latency_type type,
			  ktime_t d,
			  bool lock)
{
	unsigned long flags;

	if (lock)
		spin_lock_irqsave(&dbg_lock, flags);

	sh->states[ctarget].latency_count[type]++;

	sh->states[ctarget].latency_sum[type] =
		ktime_add(sh->states[ctarget].latency_sum[type], d);

	if (ktime_to_us(d) > ktime_to_us(sh->states[ctarget].latency_max[type]))
		sh->states[ctarget].latency_max[type] = d;

	if (ktime_to_us(d) < ktime_to_us(sh->states[ctarget].latency_min[type]))
		sh->states[ctarget].latency_min[type] = d;

	if (lock)
		spin_unlock_irqrestore(&dbg_lock, flags);
}

void ux500_ci_dbg_exit_latency(int ctarget, ktime_t now, ktime_t exit,
			       ktime_t enter,  bool timed_out)
{
	struct state_history *sh;
	bool hit = true;
	enum prcmu_idle_stat prcmu_status;
	unsigned int d;

	sh = per_cpu(state_history, smp_processor_id());

	sh->exit_counter++;

	if (timed_out)
		sh->timed_out++;

	d = ktime_to_us(ktime_sub(now, enter));

	if ((ctarget + 1) < deepest_allowed_state)
		hit = d	< cstates[ctarget + 1].threshold;
	if (d < cstates[ctarget].threshold)
		hit = false;

	if (hit)
		sh->states[ctarget].hit_rate++;

	if (cstates[ctarget].state < CI_IDLE)
		return;

	prcmu_status = ux500_pm_prcmu_idle_stat();

	switch (prcmu_status) {

	case DEEP_SLEEP_OK:
		if (cstates[ctarget].state == CI_DEEP_SLEEP)
			sh->states[ctarget].state_ok++;
		break;
	case SLEEP_OK:
		if (cstates[ctarget].state == CI_SLEEP)
			sh->states[ctarget].state_ok++;
		break;
	case IDLE_OK:
		if (cstates[ctarget].state == CI_IDLE)
			sh->states[ctarget].state_ok++;
		break;
	case DEEPIDLE_OK:
		if (cstates[ctarget].state == CI_DEEP_IDLE)
			sh->states[ctarget].state_ok++;
		break;
	case ARM2PRCMUPENDINGIT_ER:
		sh->states[ctarget].state_int++;
		break;
	default:
		pr_debug("cpuidle: unknown prcmu exit code: 0x%x state: %d\n",
			prcmu_status, cstates[ctarget].state);
		sh->states[ctarget].state_error++;
		break;
	}

	if (!measure_latency)
		return;

	store_latency(sh,
		      ctarget,
		      LATENCY_EXIT,
		      ktime_sub(now, exit),
		      true);
}

void ux500_ci_dbg_wake_latency(int ctarget, int sleep_time)
{
	struct state_history *sh;
	ktime_t l;
	ktime_t zero_time;

	if (!wake_latency || cstates[ctarget].state < CI_IDLE)
		return;

	zero_time = ktime_set(0, 0);
	sh = per_cpu(state_history, smp_processor_id());

	if (cstates[ctarget].state >= CI_SLEEP)
		l = u8500_rtc_exit_latency_get();

	if (cstates[ctarget].state == CI_IDLE) {
		ktime_t d = ktime_set(0, sleep_time * 1000);
		ktime_t now = ktime_get();

		d = ktime_add(d, sh->start);
		if (ktime_to_us(now) > ktime_to_us(d))
			l = ktime_sub(now, d);
		else
			l = zero_time;
	}

	if (!ktime_equal(zero_time, l))
		store_latency(sh,
			      ctarget,
			      LATENCY_WAKE,
			      l,
			      true);
}

static void state_record_time(struct state_history *sh, int ctarget,
			      ktime_t now, ktime_t start, bool latency)
{
	ktime_t dtime;

	dtime = ktime_sub(now, sh->start);
	sh->states[sh->state].time = ktime_add(sh->states[sh->state].time,
					       dtime);

	sh->start = now;
	sh->state = ctarget;

	if (latency && cstates[ctarget].state != CI_RUNNING && measure_latency)
		store_latency(sh,
			      ctarget,
			      LATENCY_ENTER,
			      ktime_sub(now, start),
			      false);

	sh->states[sh->state].counter++;
}

void ux500_ci_dbg_register_reason(int idx, bool power_state_req,
				  u32 time, u32 max_depth)
{


	if (cstates[idx].state == CI_IDLE) {
		apidle_ape_blocked = power_state_req;
		apidle_time_blocked = time < cstates[idx + 1].threshold;
		apidle_both_blocked = power_state_req && apidle_time_blocked;
		apidle_gov_blocked = cstates[max_depth].state == CI_IDLE;
	}
}

void ux500_ci_dbg_log(int ctarget, ktime_t enter_time)
{
	int i;
	ktime_t now;
	unsigned long flags;
	struct state_history *sh;
	struct state_history *sh_other;
	int this_cpu;

	this_cpu = smp_processor_id();

	now = ktime_get();

	sh = per_cpu(state_history, this_cpu);

	spin_lock_irqsave(&dbg_lock, flags);

	if (cstates[ctarget].state == CI_IDLE) {
		if (apidle_both_blocked)
			sh->both_blocked++;
		if (apidle_ape_blocked)
			sh->ape_blocked++;
		if (apidle_time_blocked)
			sh->time_blocked++;
		if (apidle_gov_blocked)
			sh->gov_blocked++;
	}

	/*
	 * Check if current state is just a repeat of
	 *  the state we're already in, then just quit.
	 */
	if (ctarget == sh->state)
		goto done;

	state_record_time(sh, ctarget, now, enter_time, true);

	/*
	 * Update other cpus, (this_cpu = A, other cpus = B) if:
	 * - A = running and B != WFI | running: Set B to WFI
	 * - A = WFI and then B must be running: No changes
	 * - A = !WFI && !RUNNING and then B must be WFI: B sets to A
	 */

	if (sh->state == CI_WFI)
		goto done;

	for_each_possible_cpu(i) {

		if (this_cpu == i)
			continue;

		sh_other = per_cpu(state_history, i);

		/* Same state, continue */
		if (sh_other->state == sh->state)
			continue;

		if (cstates[ctarget].state == CI_RUNNING &&
		    cstates[sh_other->state].state != CI_WFI) {
			state_record_time(sh_other, CI_WFI, now,
					  enter_time, false);
			continue;
		}
		/*
		 * This cpu is something else than running or wfi, both must be
		 * in the same state.
		 */
		state_record_time(sh_other, ctarget, now, enter_time, true);
	}
done:
	spin_unlock_irqrestore(&dbg_lock, flags);
}

static void state_history_reset(void)
{
	unsigned long flags;
	unsigned int cpu;
	int i, j;
	struct state_history *sh;

	spin_lock_irqsave(&dbg_lock, flags);

	for_each_possible_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		for (i = 0; i <= cstates_len; i++) {
			sh->states[i].counter = 0;
			sh->states[i].hit_rate = 0;
			sh->states[i].state_ok = 0;
			sh->states[i].state_error = 0;
			sh->states[i].state_int = 0;

			sh->states[i].time = ktime_set(0, 0);

			for (j = 0; j < NUM_LATENCY; j++) {
				sh->states[i].latency_count[j] = 0;
				sh->states[i].latency_min[j] = ktime_set(0,
									 10000000);
				sh->states[i].latency_max[j] = ktime_set(0, 0);
				sh->states[i].latency_sum[j] = ktime_set(0, 0);
			}
		}

		sh->start = ktime_get();
		sh->measure_begin = sh->start;
		/* Don't touch sh->state, since that is where we are now */

		sh->exit_counter = 0;
		sh->timed_out = 0;
		sh->ape_blocked = 0;
		sh->time_blocked = 0;
		sh->both_blocked = 0;
		sh->gov_blocked = 0;
	}
	spin_unlock_irqrestore(&dbg_lock, flags);
}

static ssize_t set_deepest_state(struct file *file,
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

	if (i > cstates_len - 1)
		i = cstates_len - 1;

	if (i == 0)
		i = CI_WFI;

	deepest_allowed_state = i;

	pr_debug("cpuidle: changed deepest allowed sleep state to %d.\n",
		 deepest_allowed_state);

	return buf_size;
}

static int deepest_state_print(struct seq_file *s, void *p)
{
	seq_printf(s, "Deepest allowed sleep state is %d\n",
		   deepest_allowed_state);

	return 0;
}

static ssize_t stats_write(struct file *file,
			   const char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	state_history_reset();
	return count;
}

static int wake_latency_read(struct seq_file *s, void *p)
{
	seq_printf(s, "%s\n", wake_latency ? "on" : "off");
	return 0;
}

static ssize_t wake_latency_write(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	wake_latency = !wake_latency;
	ux500_rtcrtt_measure_latency(wake_latency);
	return count;
}

static void stats_disp_one(struct seq_file *s, struct state_history *sh,
			   s64 total_us, int i)
{
	int j;
	s64 avg[NUM_LATENCY];
	s64 t_us;
	s64 perc;
	ktime_t init_time, zero_time;

	init_time = ktime_set(0, 10000000);
	zero_time = ktime_set(0, 0);

	memset(&avg, 0, sizeof(s64) * NUM_LATENCY);

	for (j = 0; j < NUM_LATENCY; j++)
		avg[j] = ktime_to_us(sh->states[i].latency_sum[j]);

	t_us = ktime_to_us(sh->states[i].time);
	perc = ktime_to_us(sh->states[i].time);
	do_div(t_us, 1000); /* to ms */
	do_div(total_us, 100);
	if (total_us)
		do_div(perc, total_us);

	for (j = 0; j < NUM_LATENCY; j++) {
		if (sh->states[i].latency_count[j])
			do_div(avg[j], sh->states[i].latency_count[j]);
	}

	seq_printf(s, "\n%d - %s: %u",
		   i, cstates[i].desc,
		   sh->states[i].counter);

	if (sh->states[i].counter == 0)
		return;

	if (i > CI_WFI)
		seq_printf(s, " (%u int:%u err:%u)",
			   sh->states[i].state_ok,
			   sh->states[i].state_int,
			   sh->states[i].state_error);

	seq_printf(s, " in %d ms %d%%",
		   (u32) t_us, (u32)perc);

	if (cstates[i].state == CI_IDLE)
		seq_printf(s, ", reg:%d time:%d both:%d gov:%d",
			   sh->ape_blocked, sh->time_blocked,
			   sh->both_blocked, sh->gov_blocked);

	if (sh->states[i].counter)
		seq_printf(s, ", hit rate: %u%% ",
			   100 * sh->states[i].hit_rate /
			   sh->states[i].counter);

	if (i == CI_RUNNING || !(measure_latency || wake_latency))
		return;

	for (j = 0; j < NUM_LATENCY; j++) {
		bool show = false;
		if (!ktime_equal(sh->states[i].latency_min[j], init_time)) {
			seq_printf(s, "\n\t\t\t\t");
			switch (j) {
			case LATENCY_ENTER:
				if (measure_latency) {
					seq_printf(s, "enter: ");
					show = true;
				}
				break;
			case LATENCY_EXIT:
				if (measure_latency) {
					seq_printf(s, "exit:	 ");
					show = true;
				}
				break;
			case LATENCY_WAKE:
				if (wake_latency) {
					seq_printf(s, "wake:	 ");
					show = true;
				}
				break;
			default:
				seq_printf(s, "unknown!: ");
				break;
			}

			if (!show)
				continue;

			if (ktime_equal(sh->states[i].latency_min[j],
					zero_time))
				seq_printf(s, "min < 30");
			else
				seq_printf(s, "min %lld",
					   ktime_to_us(sh->states[i].latency_min[j]));

			seq_printf(s, " avg %lld max %lld us, count: %d",
				   avg[j],
				   ktime_to_us(sh->states[i].latency_max[j]),
				   sh->states[i].latency_count[j]);
		}
	}
}

static int stats_print(struct seq_file *s, void *p)
{
	int cpu;
	int i;
	unsigned long flags;
	struct state_history *sh;
	ktime_t total, wall;
	s64 total_us, total_s;

	for_each_online_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		spin_lock_irqsave(&dbg_lock, flags);
		seq_printf(s, "\nCPU%d\n", cpu);

		total = ktime_set(0, 0);

		for (i = 0; i < cstates_len; i++)
			total = ktime_add(total, sh->states[i].time);

		wall = ktime_sub(ktime_get(), sh->measure_begin);

		total_us = ktime_to_us(wall);
		total_s = ktime_to_ms(wall);

		do_div(total_s, 1000);

		if (total_s)
			seq_printf(s,
				   "wake ups per s: %u.%u timed out: %u%%\n",
				   sh->exit_counter / (int) total_s,
				   (10 * sh->exit_counter / (int) total_s) -
				   10 * (sh->exit_counter / (int) total_s),
				   100 * sh->timed_out / sh->exit_counter);

		seq_printf(s, "delta accounted vs wall clock: %lld us\n",
			   ktime_to_us(ktime_sub(wall, total)));

		for (i = 0; i < cstates_len; i++)
			stats_disp_one(s, sh, total_us, i);

		seq_printf(s, "\n");
		spin_unlock_irqrestore(&dbg_lock, flags);
	}
	seq_printf(s, "\n");
	return 0;
}


static int ap_family_show(struct seq_file *s, void *iter)
{
	int i;
	u32 count = 0;
	unsigned long flags;
	struct state_history *sh;

	sh = per_cpu(state_history, 0);
	spin_lock_irqsave(&dbg_lock, flags);

	for (i = 0 ; i < cstates_len; i++) {
		if (cstates[i].state == (enum ci_pwrst)s->private)
			count += sh->states[i].counter;
	}

	seq_printf(s, "%u\n", count);
	spin_unlock_irqrestore(&dbg_lock, flags);

	return 0;
}

static int deepest_state_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, deepest_state_print, inode->i_private);
}

static int stats_open_file(struct inode *inode, struct file *file)
{
	return single_open(file, stats_print, inode->i_private);
}

static int ap_family_open(struct inode *inode,
			  struct file *file)
{
	return single_open(file, ap_family_show, inode->i_private);
}

static int wake_latency_open(struct inode *inode,
			  struct file *file)
{
	return single_open(file, wake_latency_read, inode->i_private);
}

static const struct file_operations deepest_state_fops = {
	.open = deepest_state_open_file,
	.write = set_deepest_state,
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

static const struct file_operations ap_family_fops = {
	.open = ap_family_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations wake_latency_fops = {
	.open = wake_latency_open,
	.write = wake_latency_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static struct dentry *cpuidle_dir;

static void setup_debugfs(void)
{
	cpuidle_dir = debugfs_create_dir("cpuidle", NULL);
	if (IS_ERR_OR_NULL(cpuidle_dir))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_file("deepest_state",
					       S_IWUGO | S_IRUGO, cpuidle_dir,
					       NULL, &deepest_state_fops)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_file("stats",
					       S_IRUGO, cpuidle_dir, NULL,
					       &stats_fops)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_bool("dbg_console_enable",
					       S_IWUGO | S_IRUGO, cpuidle_dir,
					       &dbg_console_enable)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_bool("measure_latency",
					       S_IWUGO | S_IRUGO, cpuidle_dir,
					       &measure_latency)))
		goto fail;


	if (IS_ERR_OR_NULL(debugfs_create_file("wake_latency",
					       S_IWUGO | S_IRUGO, cpuidle_dir,
					       NULL,
					       &wake_latency_fops)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_file("ap_idle", S_IRUGO,
					       cpuidle_dir,
					       (void *)CI_IDLE,
					       &ap_family_fops)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_file("ap_sleep", S_IRUGO,
					       cpuidle_dir,
					       (void *)CI_SLEEP,
					       &ap_family_fops)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_file("ap_deepidle", S_IRUGO,
					       cpuidle_dir,
					       (void *)CI_DEEP_IDLE,
					       &ap_family_fops)))
		goto fail;

	if (IS_ERR_OR_NULL(debugfs_create_file("ap_deepsleep", S_IRUGO,
					       cpuidle_dir,
					       (void *)CI_DEEP_SLEEP,
					       &ap_family_fops)))
		goto fail;

	return;
fail:
	debugfs_remove_recursive(cpuidle_dir);
}

void ux500_ci_dbg_init(void)
{
	char clkname[10];
	int cpu;

	struct state_history *sh;

	cstates = ux500_ci_get_cstates(&cstates_len);

	if (deepest_allowed_state > cstates_len)
		deepest_allowed_state = cstates_len;

	for_each_possible_cpu(cpu) {
		per_cpu(state_history, cpu) = kzalloc(sizeof(struct state_history),
						      GFP_KERNEL);
		sh = per_cpu(state_history, cpu);
		sh->states = kzalloc(sizeof(struct state_history_state)
				     * cstates_len,
				     GFP_KERNEL);
	}

	state_history_reset();

	for_each_possible_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		/* Only first CPU used during boot */
		if (cpu == 0)
			sh->state = CI_RUNNING;
		else
			sh->state = CI_WFI;
	}

	setup_debugfs();

	/* Uart debug init */
	switch (CONFIG_UX500_DEBUG_UART) {
	case 0:
		uart_base = ioremap(U8500_UART0_BASE, SZ_4K);
		break;
	case 1:
		uart_base = ioremap(U8500_UART1_BASE, SZ_4K);
		break;
	case 2:
		uart_base = ioremap(U8500_UART2_BASE, SZ_4K);
		break;
	default:
		uart_base = ioremap(U8500_UART2_BASE, SZ_4K);
		break;
	}

	snprintf(clkname, sizeof(clkname), "uart%d", CONFIG_UX500_DEBUG_UART);
	uart_clk = clk_get_sys(clkname, NULL);
	BUG_ON(IS_ERR(uart_clk));

	INIT_DELAYED_WORK_DEFERRABLE(&cpuidle_work, dbg_cpuidle_work_function);

}

void ux500_ci_dbg_remove(void)
{
	int cpu;
	struct state_history *sh;

	debugfs_remove_recursive(cpuidle_dir);

	for_each_possible_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		kfree(sh->states);
		kfree(sh);
	}

	iounmap(uart_base);
}
