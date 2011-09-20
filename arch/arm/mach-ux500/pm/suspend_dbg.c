/*
 * Copyright (C) ST-Ericsson SA 2010-2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>,
 *	   Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 *
 */
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include <mach/pm.h>

#ifdef CONFIG_UX500_SUSPEND_DBG_WAKE_ON_UART
void ux500_suspend_dbg_add_wake_on_uart(void)
{
	irq_set_irq_wake(GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 1);
	irq_set_irq_type(GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN),
		     IRQ_TYPE_EDGE_BOTH);
}

void ux500_suspend_dbg_remove_wake_on_uart(void)
{
	irq_set_irq_wake(GPIO_TO_IRQ(CONFIG_UX500_CONSOLE_UART_GPIO_PIN), 0);
}
#endif

void ux500_suspend_dbg_sleep_status(bool is_deepsleep)
{
	enum prcmu_idle_stat prcmu_status;

	prcmu_status = ux500_pm_prcmu_idle_stat();

	if (is_deepsleep) {
		pr_info("Returning from ApDeepSleep. PRCMU ret: 0x%x - %s\n",
			prcmu_status,
			prcmu_status == DEEP_SLEEP_OK ? "Success" : "Fail!");
	} else {
		pr_info("Returning from ApSleep. PRCMU ret: 0x%x - %s\n",
			prcmu_status,
			prcmu_status == SLEEP_OK ? "Success" : "Fail!");
	}
}
