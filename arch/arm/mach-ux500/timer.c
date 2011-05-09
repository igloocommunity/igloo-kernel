/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 * Author: Mattias Wallin <mattias.wallin@stericsson.com> for ST-Ericsson
 */
#include <linux/io.h>
#include <mach/setup.h>
#include <mach/hardware.h>

#include "timer-rtt.h"
#include "pm/context.h"

void prcmu_timer_init(void);
void mtu_timer_init(void);
void mtu_timer_reset(void);

#ifdef CONFIG_LOCAL_TIMERS
#include <asm/smp_twd.h>
#endif

#ifdef CONFIG_UX500_CONTEXT
static int mtu_context_notifier_call(struct notifier_block *this,
				     unsigned long event, void *data)
{
	if (event == CONTEXT_APE_RESTORE)
		mtu_timer_reset();
	return NOTIFY_OK;
}

static struct notifier_block mtu_context_notifier = {
	.notifier_call = mtu_context_notifier_call,
};
#endif

static void ux500_timer_reset(void)
{
	mtu_timer_reset();
}

static void __init ux500_timer_init(void)
{

#ifdef CONFIG_LOCAL_TIMERS
	if (cpu_is_u5500())
		twd_base = __io_address(U5500_TWD_BASE);
	else if (cpu_is_u8500())
		twd_base = __io_address(U8500_TWD_BASE);
	else
		ux500_unknown_soc();
#endif
/*
 * Here we register the timerblocks active in the system.
 * Localtimers (twd) is started when both cpu is up and running.
 * MTU register a clocksource, clockevent and sched_clock.
 * Since the MTU is located in the VAPE power domain
 * it will be cleared in sleep which makes it unsuitable.
 * We however need it as a timer tick (clockevent)
 * during boot to calibrate delay until twd is started.
 * RTC-RTT have problems as timer tick during boot since it is depending
 * on delay which is not yet calibrated. RTC-RTT is in the always-on
 * powerdomain and is used as clockevent instead of twd when sleeping.
 * The PRCMU timer 4 register a clocksource and sched_clock with higher
 * rating then MTU since is always-on.
 *
 */
	mtu_timer_init();
#ifdef CONFIG_UX500_PRCMU_TIMER
	rtc_rtt_timer_init(0);
	prcmu_timer_init();
#endif

#ifdef CONFIG_UX500_CONTEXT
	WARN_ON(context_ape_notifier_register(&mtu_context_notifier));
#endif
}

struct sys_timer ux500_timer = {
	.init		= ux500_timer_init,
	.resume		= ux500_timer_reset,
};
