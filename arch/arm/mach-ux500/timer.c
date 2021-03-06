/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 * Author: Mattias Wallin <mattias.wallin@stericsson.com> for ST-Ericsson
 */
#include <linux/io.h>
#include <linux/clksrc-dbx500-prcmu.h>

#include <asm/localtimer.h>

#include <plat/mtu.h>

#include <mach/setup.h>
#include <mach/hardware.h>
#include <mach/context.h>

#ifdef CONFIG_UX500_CONTEXT
static int mtu_context_notifier_call(struct notifier_block *this,
				     unsigned long event, void *data)
{
	if (event == CONTEXT_APE_RESTORE)
		nmdk_clksrc_reset();
	return NOTIFY_OK;
}

static struct notifier_block mtu_context_notifier = {
	.notifier_call = mtu_context_notifier_call,
};
#endif

static void ux500_timer_reset(void)
{
	nmdk_clkevt_reset();
}

static void __init ux500_timer_init(void)
{
	if (cpu_is_u5500()) {
#ifdef CONFIG_LOCAL_TIMERS
		twd_base = __io_address(U5500_TWD_BASE);
#endif
		mtu_base = __io_address(U5500_MTU0_BASE);
		clksrc_dbx500_timer_base = __io_address(U5500_PRCMU_TIMER_3_BASE);
	} else if (cpu_is_u8500()) {
#ifdef CONFIG_LOCAL_TIMERS
		twd_base = __io_address(U8500_TWD_BASE);
#endif
		mtu_base = __io_address(U8500_MTU0_BASE);
		clksrc_dbx500_timer_base = __io_address(U8500_PRCMU_TIMER_4_BASE);
	} else {
		ux500_unknown_soc();
	}

	/*
	 * Here we register the timerblocks active in the system.
	 * Localtimers (twd) is started when both cpu is up and running.
	 * MTU register a clocksource, clockevent and sched_clock.
	 * Since the MTU is located in the VAPE power domain
	 * it will be cleared in sleep which makes it unsuitable.
	 * We however need it as a timer tick (clockevent)
	 * during boot to calibrate delay until twd is started.
	 * RTC-RTT have problems as timer tick during boot since it is
	 * depending on delay which is not yet calibrated. RTC-RTT is in the
	 * always-on powerdomain and is used as clockevent instead of twd when
	 * sleeping.
	 * The PRCMU timer 4(3 for DB5500) register a clocksource and
	 * sched_clock with higher rating then MTU since is always-on.
	 *
	 */

	nmdk_timer_init();
	clksrc_dbx500_prcmu_init();

#ifdef CONFIG_UX500_CONTEXT
	WARN_ON(context_ape_notifier_register(&mtu_context_notifier));
#endif

}

struct sys_timer ux500_timer = {
	.init		= ux500_timer_init,
	.resume		= ux500_timer_reset,
};
