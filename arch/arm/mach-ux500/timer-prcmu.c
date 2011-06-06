/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 * Author: Mattias Wallin <mattias.wallin@stericsson.com> for ST-Ericsson
 * sched_clock implementation is based on:
 * plat-nomadik/timer.c Linus Walleij <linus.walleij@stericsson.com>
 *
 * DBx500-PRCMU Timer
 * The PRCMU has 5 timers which are available in a always-on
 * power domain.  We use the Timer 4 for our always-on clock
 * source on DB8500 and Timer 3 on DB5500.
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

#define PRCMU_TIMER_3_BASE       0x338
#define PRCMU_TIMER_4_BASE       0x450

#define PRCMU_TIMER_REF       0x0
#define PRCMU_TIMER_DOWNCOUNT 0x4
#define PRCMU_TIMER_MODE      0x8

static __iomem void *timer_base;

#define SCHED_CLOCK_MIN_WRAP (131072) /* 2^32 / 32768 */

static cycle_t prcmu_read_timer_nop(struct clocksource *cs)
{
	return 0;
}

static struct clocksource prcmu_clksrc = {
	.name		= "prcmu-timer",
	.rating		= 300,
	.read		= prcmu_read_timer_nop,
	.shift		= 10,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static cycle_t prcmu_read_timer(struct clocksource *cs)
{
	u32 count, count2;

	do {
		count = readl(timer_base + PRCMU_TIMER_DOWNCOUNT);
		count2 = readl(timer_base + PRCMU_TIMER_DOWNCOUNT);
	} while (count2 != count);

	/*
	 * clocksource: the prcmu timer is a decrementing counters, so we negate
	 * the value being read.
	 */
	return ~count;
}

#ifdef CONFIG_UX500_PRCMU_TIMER
unsigned long long notrace sched_clock(void)
{
	return clocksource_cyc2ns(prcmu_clksrc.read(
			&prcmu_clksrc),
			prcmu_clksrc.mult,
			prcmu_clksrc.shift);
}
#endif

#ifdef CONFIG_BOOTTIME

static unsigned long __init boottime_get_time(void)
{
	return div_s64(clocksource_cyc2ns(prcmu_clksrc.read(
			&prcmu_clksrc),
			prcmu_clksrc.mult,
			prcmu_clksrc.shift), 1000);
}

static struct boottime_timer __initdata boottime_timer = {
	.init     = NULL,
	.get_time = boottime_get_time,
	.finalize = NULL,
};
#endif

void __init prcmu_timer_init(void)
{
	void __iomem *prcmu_base;

	if (ux500_is_svp())
		return;

	if (cpu_is_u8500()) {
		prcmu_base = __io_address(U8500_PRCMU_BASE);
		timer_base = prcmu_base + PRCMU_TIMER_4_BASE;
	} else if (cpu_is_u5500()) {
		prcmu_base = __io_address(U5500_PRCMU_BASE);
		timer_base = prcmu_base + PRCMU_TIMER_3_BASE;
	} else
		ux500_unknown_soc();

	clocksource_calc_mult_shift(&prcmu_clksrc,
		RATE_32K, SCHED_CLOCK_MIN_WRAP);

	/*
	 * The A9 sub system expects the timer to be configured as
	 * a continous looping timer.
	 * The PRCMU should configure it but if it for some reason
	 * don't we do it here.
	 */
	if (readl(timer_base + PRCMU_TIMER_MODE) != TIMER_MODE_CONTINOUS) {
		writel(TIMER_MODE_CONTINOUS, timer_base + PRCMU_TIMER_MODE);
		writel(TIMER_DOWNCOUNT_VAL, timer_base + PRCMU_TIMER_REF);
	}
	prcmu_clksrc.read = prcmu_read_timer;

	clocksource_register(&prcmu_clksrc);

	if (!ux500_is_svp())
		boottime_activate(&boottime_timer);
}

