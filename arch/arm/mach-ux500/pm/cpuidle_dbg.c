/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 */

#include <linux/slab.h>
#include <linux/sched.h>
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

#include <plat/gpio.h>
#include <asm/hardware/gic.h>

#include "cpuidle.h"
#include "pm.h"

#define DBG_BUF_SIZE 5000
#define APE_ON_TIMER_INTERVAL 5 /* Seconds */

#define UART_RX_GPIO_PIN_MASK (1 << (CONFIG_UX500_CONSOLE_UART_GPIO_PIN % 32))

#define UART011_MIS_RTIS (1 << 6) /* receive timeout interrupt status */
#define UART011_MIS_RXIS (1 << 4) /* receive interrupt status */
#define UART011_MIS 0x40 /* Masked interrupt status register */

struct state_history {
	ktime_t start;
	u32 state;
	u32 *counter;
	ktime_t *time;
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

static struct cstate *cstates;
static int cstates_len;
static DEFINE_SPINLOCK(dbg_lock);

#ifdef U8500_CPUIDLE_EXTRA_DBG
void ux500_ci_dbg_msg(char *dbg_string)
{
	static char dbg_buf[DBG_BUF_SIZE];
	static int index; /* protected by dbg_lock */
	int str_len;
	int smp_no_len;
	int head_len;
	unsigned long flags;
	static const char * const smp_no_str = "\n  %d:";
	static const char * const head_str = ":HEAD->";

	spin_lock_irqsave(&dbg_lock, flags);

	str_len = strlen(dbg_string);
	smp_no_len = strlen(smp_no_str);
	head_len = strlen(head_str);

	if (index > head_len)
		/* Remove last head printing */
		index -= head_len;

	if ((index + str_len + smp_no_len + head_len) > DBG_BUF_SIZE)
		index = 0; /* Non perfect wrapping... */

	sprintf(&dbg_buf[index], smp_no_str, smp_processor_id());
	index += smp_no_len;

	strcpy(&dbg_buf[index], dbg_string);
	index += str_len;

	strcpy(&dbg_buf[index], head_str);
	index += head_len;

	spin_unlock_irqrestore(&dbg_lock, flags);
}
#endif

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

static void state_record_time(struct state_history *sh, enum ci_pwrst pstate,
			      ktime_t now)
{
	ktime_t dtime;

	dtime = ktime_sub(now, sh->start);
	sh->time[sh->state] = ktime_add(sh->time[sh->state], dtime);

	sh->start = now;

	sh->state = pstate;
	sh->counter[sh->state]++;
}

void ux500_ci_dbg_log(enum ci_pwrst pstate, int this_cpu)
{
	int i;
	ktime_t now;

	unsigned long flags;
	struct state_history *sh;
	struct state_history *sh_other;

	now = ktime_get();

	sh = per_cpu(state_history, this_cpu);

	spin_lock_irqsave(&dbg_lock, flags);

	/*
	 * Check if current state is just a repeat of
	 *  the state we're already in, then just quit.
	 */
	if (pstate == sh->state)
		goto done;

	state_record_time(sh, pstate, now);

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

		if (pstate == CI_RUNNING && sh_other->state != CI_WFI) {
			state_record_time(sh_other, CI_WFI, now);
			continue;
		}
		/*
		 * This cpu is something else than running or wfi, both must be
		 * in the same state.
		 */
		state_record_time(sh_other, pstate, now);
	}
done:
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
		i = 1;

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
	unsigned long flags;
	unsigned int cpu;
	int i;
	struct state_history *sh;

	printk(KERN_INFO "\nreset\n");

	for_each_possible_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		spin_lock_irqsave(&dbg_lock, flags);
		for (i = 0; i < cstates_len; i++) {
			sh->counter[i] = 0;
			sh->time[i] = ktime_set(0, 0);
			sh->start = ktime_get();
		}

		sh->state = CI_RUNNING;
		spin_unlock_irqrestore(&dbg_lock, flags);
	}

	return count;
}

static int stats_print(struct seq_file *s, void *p)
{
	int cpu;
	int i;
	unsigned long flags;
	struct state_history *sh;
	ktime_t total;
	s64 t_us;
	s64 perc;
	s64 total_us;

	for_each_possible_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		spin_lock_irqsave(&dbg_lock, flags);
		seq_printf(s, "\nCPU%d\n", cpu);

		total = ktime_set(0, 0);

		for (i = 0; i < cstates_len; i++)
			total = ktime_add(total, sh->time[i]);
		total_us = ktime_to_us(total);
		do_div(total_us, 100);

		for (i = 0; i < cstates_len; i++) {

			t_us = ktime_to_us(sh->time[i]);
			perc = ktime_to_us(sh->time[i]);
			do_div(t_us, 1000); /* to ms */
			if (total_us != 0)
				do_div(perc, total_us);

			seq_printf(s, "%d - %s: # %u in %d ms %d%%\n",
				   i, cstates[i].desc,
				   sh->counter[i],
				   (u32) t_us, (u32)perc);
		}
		spin_unlock_irqrestore(&dbg_lock, flags);
	}
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
			count += sh->counter[i];
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

	for_each_possible_cpu(cpu) {
		per_cpu(state_history, cpu) = kzalloc(sizeof(struct state_history),
						      GFP_KERNEL);
		sh = per_cpu(state_history, cpu);
		sh->counter = kzalloc(sizeof(u32) * cstates_len,
				      GFP_KERNEL);
		sh->time = kzalloc(sizeof(ktime_t) * cstates_len,
				   GFP_KERNEL);

		/* Only first CPU used during boot */
		if (cpu == 0)
			sh->state = CI_RUNNING;
		else
			sh->state = CI_WFI;
		sh->start = ktime_get();
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

	INIT_DELAYED_WORK(&cpuidle_work, dbg_cpuidle_work_function);

}

void ux500_ci_dbg_remove(void)
{
	int cpu;
	struct state_history *sh;

	debugfs_remove_recursive(cpuidle_dir);

	for_each_possible_cpu(cpu) {
		sh = per_cpu(state_history, cpu);
		kfree(sh->time);
		kfree(sh->counter);
		kfree(sh);
	}

	iounmap(uart_base);
}
