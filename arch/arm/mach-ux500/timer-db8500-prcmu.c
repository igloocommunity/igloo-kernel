/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 * Author: Mattias Wallin <mattias.wallin@stericsson.com> for ST-Ericsson
 * sched_clock implementation is based on:
 * plat-nomadik/timer.c Linus Walleij <linus.walleij@stericsson.com>
 *
 * DB8500-PRCMU Timer
 * The PRCMU has 5 timers which are available in a always-on
 * power domain. we use the Timer 4 for our always-on clock source.
 */
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/boottime.h>
#include <linux/cnt32_to_63.h>
#include <linux/sched.h>
#include <mach/setup.h>
#include <mach/db8500-regs.h>
#include <mach/hardware.h>

#define RATE_32K		(32768)

#define TIMER_MODE_CONTINOUS	(0x1)
#define TIMER_DOWNCOUNT_VAL	(0xffffffff)

/* PRCMU Timer 4 */
#define PRCMU_TIMER_4_REF       (prcmu_base + 0x450)
#define PRCMU_TIMER_4_DOWNCOUNT (prcmu_base + 0x454)
#define PRCMU_TIMER_4_MODE      (prcmu_base + 0x458)

static __iomem void *prcmu_base;

#define SCHED_CLOCK_MIN_WRAP (131072) /* 2^32 / 32768 */

static cycle_t db8500_prcmu_read_timer_nop(struct clocksource *cs)
{
	return 0;
}

static struct clocksource db8500_prcmu_clksrc = {
	.name		= "db8500-prcmu-timer4",
	.rating		= 300,
	.read		= db8500_prcmu_read_timer_nop,
	.shift		= 10,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static cycle_t db8500_prcmu_read_timer(struct clocksource *cs)
{
	u32 count, count2;

	do {
		count = readl(PRCMU_TIMER_4_DOWNCOUNT);
		count2 = readl(PRCMU_TIMER_4_DOWNCOUNT);
	} while (count2 != count);

	/*
	 * clocksource: the prcmu timer is a decrementing counters, so we negate
	 * the value being read.
	 */
	return ~count;
}

#ifdef CONFIG_U8500_PRCMU_TIMER
unsigned long long notrace sched_clock(void)
{
	return clocksource_cyc2ns(db8500_prcmu_clksrc.read(
			&db8500_prcmu_clksrc),
			db8500_prcmu_clksrc.mult,
			db8500_prcmu_clksrc.shift);
}
#endif

#ifdef CONFIG_BOOTTIME

static unsigned long __init boottime_get_time(void)
{
	return div_s64(clocksource_cyc2ns(db8500_prcmu_clksrc.read(
			&db8500_prcmu_clksrc),
			db8500_prcmu_clksrc.mult,
			db8500_prcmu_clksrc.shift), 1000);
}

static struct boottime_timer __initdata boottime_timer = {
	.init     = NULL,
	.get_time = boottime_get_time,
	.finalize = NULL,
};
#endif

void __init db8500_prcmu_timer_init(void)
{
	if (ux500_is_svp())
		return;

	prcmu_base = __io_address(U8500_PRCMU_BASE);

	clocksource_calc_mult_shift(&db8500_prcmu_clksrc,
		RATE_32K, SCHED_CLOCK_MIN_WRAP);

	/*
	 * The A9 sub system expects the timer to be configured as
	 * a continous looping timer.
	 * The PRCMU should configure it but if it for some reason
	 * don't we do it here.
	 */
	if (readl(PRCMU_TIMER_4_MODE) != TIMER_MODE_CONTINOUS) {
		writel(TIMER_MODE_CONTINOUS, PRCMU_TIMER_4_MODE);
		writel(TIMER_DOWNCOUNT_VAL, PRCMU_TIMER_4_REF);
	}
	db8500_prcmu_clksrc.read = db8500_prcmu_read_timer;

	clocksource_register(&db8500_prcmu_clksrc);

	if (!ux500_is_svp())
		boottime_activate(&boottime_timer);
}

