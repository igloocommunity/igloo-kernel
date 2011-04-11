/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define DEEP_SLEEP_OK 0xf6
#define SLEEP_OK 0xf3

#define PRCMU_STATUS_REGISTER_V1 0x8015fe08
#define PRCMU_STATUS_REGISTER_V2 0x801b8e08

static void __iomem *prcmu_status_reg;

#ifdef CONFIG_UX500_SUSPEND_STANDBY
static u32 sleep_enabled = 1;
#else
static u32 sleep_enabled;
#endif

#ifdef CONFIG_UX500_SUSPEND_MEM
static u32 deepsleep_enabled = 1;
#else
static u32 deepsleep_enabled;
#endif

static u32 suspend_enabled = 1;

static u32 deepsleeps_done;
static u32 deepsleeps_failed;
static u32 sleeps_done;
static u32 sleeps_failed;
static u32 suspend_count;

#ifdef CONFIG_UX500_SUSPEND_DBG_WAKE_ON_UART
void ux500_suspend_dbg_add_wake_on_uart(void)
{
	set_irq_wake(NOMADIK_GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 1);
	set_irq_type(NOMADIK_GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN),
		     IRQ_TYPE_EDGE_BOTH);
}

void ux500_suspend_dbg_remove_wake_on_uart(void)
{
	set_irq_wake(NOMADIK_GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 0);
}
#endif

bool ux500_suspend_enabled(void)
{
	return suspend_enabled != 0;
}

bool ux500_suspend_sleep_enabled(void)
{
	return sleep_enabled != 0;
}

bool ux500_suspend_deepsleep_enabled(void)
{
	return deepsleep_enabled != 0;
}

void ux500_suspend_dbg_sleep_status(bool is_deepsleep)
{
	u32 prcmu_status;

	prcmu_status = readl(prcmu_status_reg) & 0xff;

	if (is_deepsleep) {
		pr_info("Returning from ApDeepSleep. PRCMU ret: 0x%x - %s\n",
			prcmu_status,
			prcmu_status == DEEP_SLEEP_OK ? "Success" : "Fail!");
		if (prcmu_status == DEEP_SLEEP_OK)
			deepsleeps_done++;
		else
			deepsleeps_failed++;
	} else {
		pr_info("Returning from ApSleep. PRCMU ret: 0x%x - %s\n",
			prcmu_status,
			prcmu_status == SLEEP_OK ? "Success" : "Fail!");
		if (prcmu_status == SLEEP_OK)
			sleeps_done++;
		else
			sleeps_failed++;
	}
}

int ux500_suspend_dbg_begin(suspend_state_t state)
{
	suspend_count++;
	return 0;
}

void ux500_suspend_dbg_init(void)
{
	struct dentry *suspend_dir = NULL;
	struct dentry *sleep_file = NULL;
	struct dentry *deepsleep_file = NULL;
	struct dentry *enable_file = NULL;
	struct dentry *suspend_count_file = NULL;
	struct dentry *sleeps_done_file = NULL;
	struct dentry *deepsleeps_done_file = NULL;
	struct dentry *sleeps_failed_file = NULL;
	struct dentry *deepsleeps_failed_file = NULL;


	if (cpu_is_u8500v20_or_later())
		prcmu_status_reg = (void *)IO_ADDRESS(PRCMU_STATUS_REGISTER_V2);
	else
		prcmu_status_reg = (void *)IO_ADDRESS(PRCMU_STATUS_REGISTER_V1);

	suspend_dir = debugfs_create_dir("suspend", NULL);
	if (IS_ERR_OR_NULL(suspend_dir))
		return;

	sleep_file = debugfs_create_bool("sleep", S_IWUGO | S_IRUGO,
					 suspend_dir,
					 &sleep_enabled);
	if (IS_ERR_OR_NULL(sleep_file))
		goto error;

	deepsleep_file = debugfs_create_bool("deepsleep", S_IWUGO | S_IRUGO,
					     suspend_dir,
					     &deepsleep_enabled);
	if (IS_ERR_OR_NULL(deepsleep_file))
		goto error;

	enable_file = debugfs_create_bool("enable", S_IWUGO | S_IRUGO,
					  suspend_dir,
					  &suspend_enabled);
	if (IS_ERR_OR_NULL(enable_file))
		goto error;

	suspend_count_file = debugfs_create_u32("count", S_IRUGO,
						suspend_dir,
						&suspend_count);
	if (IS_ERR_OR_NULL(suspend_count_file))
		goto error;

	sleeps_done_file = debugfs_create_u32("sleep_count", S_IRUGO,
					      suspend_dir,
					      &sleeps_done);
	if (IS_ERR_OR_NULL(sleeps_done_file))
		goto error;

	deepsleeps_done_file = debugfs_create_u32("deepsleep_count", S_IRUGO,
						  suspend_dir,
						  &deepsleeps_done);
	if (IS_ERR_OR_NULL(deepsleeps_done_file))
		goto error;


	sleeps_failed_file = debugfs_create_u32("sleep_failed", S_IRUGO,
						suspend_dir,
						&sleeps_failed);
	if (IS_ERR_OR_NULL(sleeps_failed_file))
		goto error;

	deepsleeps_failed_file = debugfs_create_u32("deepsleep_failed", S_IRUGO,
						    suspend_dir,
						    &deepsleeps_failed);
	if (IS_ERR_OR_NULL(deepsleeps_failed_file))
		goto error;

	return;
error:
	if (!IS_ERR_OR_NULL(deepsleeps_failed_file))
		debugfs_remove(deepsleeps_failed_file);
	if (!IS_ERR_OR_NULL(sleeps_failed_file))
		debugfs_remove(sleeps_failed_file);
	if (!IS_ERR_OR_NULL(deepsleeps_done_file))
		debugfs_remove(deepsleeps_done_file);
	if (!IS_ERR_OR_NULL(sleeps_done_file))
		debugfs_remove(sleeps_done_file);
	if (!IS_ERR_OR_NULL(suspend_count_file))
		debugfs_remove(suspend_count_file);
	if (!IS_ERR_OR_NULL(enable_file))
		debugfs_remove(enable_file);
	if (!IS_ERR_OR_NULL(deepsleep_file))
		debugfs_remove(deepsleep_file);
	if (!IS_ERR_OR_NULL(sleep_file))
		debugfs_remove(sleep_file);

	debugfs_remove(suspend_dir);
}
