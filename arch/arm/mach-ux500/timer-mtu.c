/*
 * linux/arch/arm/mach-u8500/timer.c
 *
 * Copyright (C) 2008 STMicroelectronics
 * Copyright (C) 2009 Alessandro Rubini, somewhat based on at91sam926x
 * Copyright (C) 2009 ST-Ericsson SA
 *	added support to u8500 platform, heavily based on 8815
 * 	Author: Srinidhi KASAGAR <srinidhi.kasagar@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/boottime.h>
#include <linux/cnt32_to_63.h>
#include <asm/mach/time.h>
#include <plat/mtu.h>
#include <mach/setup.h>

#define TIMER_CTRL	0x80	/* No divisor */
#define TIMER_PERIODIC	0x40
#define TIMER_SZ32BIT	0x02

#define MTU_CRn_DIS		(0x00 << 7)
#define MTU_CRn_FREERUNNING	(0x00 << 6)

static u32 u8500_cycle;		/* write-once */
static __iomem void *mtu0_base;
static bool mtu_periodic;

/*
 * U8500 sched_clock implementation. It has a resolution of
 * at least 7.5ns (133MHz MTU rate) and a maximum value of 834 days.
 *
 * Because the hardware timer period is quite short (32.3 secs
 * and because cnt32_to_63() needs to be called at
 * least once per half period to work properly, a kernel timer is
 * set up to ensure this requirement is always met.
 *
 * Based on plat-orion time.c implementation.
 */
#define TCLK2NS_SCALE_FACTOR 8

#ifdef CONFIG_UX500_MTU_TIMER
static unsigned long tclk2ns_scale;
static struct timer_list cnt32_to_63_keepwarm_timer;

unsigned long long sched_clock(void)
{
	unsigned long long v;

	if (unlikely(!mtu0_base))
		return 0;

	v = cnt32_to_63(0xffffffff - readl(mtu0_base + MTU_VAL(1)));
	return (v * tclk2ns_scale) >> TCLK2NS_SCALE_FACTOR;
}

static void cnt32_to_63_keepwarm(unsigned long data)
{
	mod_timer(&cnt32_to_63_keepwarm_timer, round_jiffies(jiffies + data));
	(void) sched_clock();
}

static void __init setup_sched_clock(unsigned long tclk)
{
	unsigned long long v;
	unsigned long data;

	v = NSEC_PER_SEC;
	v <<= TCLK2NS_SCALE_FACTOR;
	v += tclk / 2;
	do_div(v, tclk);
	/*
	 * We want an even value to automatically clear the top bit
	 * returned by cnt32_to_63() without an additional run time
	 * instruction. So if the LSB is 1 then round it up.
	 */
	if (v & 1)
		v++;
	tclk2ns_scale = v;

	data = (0xffffffffUL / tclk / 2 - 2) * HZ;
	setup_timer(&cnt32_to_63_keepwarm_timer, cnt32_to_63_keepwarm, data);
	mod_timer(&cnt32_to_63_keepwarm_timer, round_jiffies(jiffies + data));
}
#else
static void __init setup_sched_clock(unsigned long tclk)
{
}
#endif
/*
 * clocksource: the MTU device is a decrementing counters, so we negate
 * the value being read.
 */
static cycle_t u8500_read_timer(struct clocksource *cs)
{
	u32 count = readl(mtu0_base + MTU_VAL(1));
	return ~count;
}
/*
 * Kernel assumes that sched_clock can be called early
 * but the MTU may not yet be initialized.
 */
static cycle_t u8500_read_timer_dummy(struct clocksource *cs)
{
	return 0;
}
void mtu_clockevent_reset(void);
void mtu_clocksource_reset(void)
{
	writel(MTU_CRn_DIS, mtu0_base + MTU_CR(1));

	/* ClockSource: configure load and background-load, and fire it up */
	writel(u8500_cycle, mtu0_base + MTU_LR(1));
	writel(u8500_cycle, mtu0_base + MTU_BGLR(1));

	writel(MTU_CRn_PRESCALE_1 | MTU_CRn_32BITS | MTU_CRn_ENA |
	       MTU_CRn_FREERUNNING, mtu0_base + MTU_CR(1));
}

static void u8500_mtu_clocksource_resume(struct clocksource *cs)
{
	mtu_clocksource_reset();
	mtu_clockevent_reset();
}

static struct clocksource u8500_clksrc = {
	.name		= "mtu_1",
	.rating		= 120,
	.read		= u8500_read_timer_dummy,
	.shift		= 20,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.resume		= u8500_mtu_clocksource_resume,
};

#ifdef ARCH_HAS_READ_CURRENT_TIMER
void mtu_timer_delay_loop(unsigned long loops)
{
	unsigned long bclock, now;

	bclock = u8500_read_timer(&u8500_clksrc);
	do {
		now = u8500_read_timer(&u8500_clksrc);
		/* If timer have been cleared (suspend) or wrapped we exit */
		if (unlikely(now < bclock))
			return;
	} while ((now - bclock) < loops);
}

/* Used to calibrate the delay */
int read_current_timer(unsigned long *timer_val)
{
	*timer_val = u8500_read_timer(&u8500_clksrc);
	return 0;
}
#endif

/*
 * Clockevent device: currently only periodic mode is supported
 */

static void u8500_mtu_clkevt_mode(enum clock_event_mode mode,
			     struct clock_event_device *dev)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		mtu_periodic = true;
		mtu_clockevent_reset();
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		mtu_periodic = false;
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		writel(MTU_CRn_DIS, mtu0_base + MTU_CR(0));
		writel(0, mtu0_base + MTU_IMSC);
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static int u8500_mtu_clkevt_next(unsigned long evt, struct clock_event_device *ev)
{
	writel(1 << 0, mtu0_base + MTU_IMSC);
	writel(evt, mtu0_base + MTU_LR(0));
	/* Load highest value, enable device, enable interrupts */
	writel(MTU_CRn_ONESHOT | MTU_CRn_PRESCALE_1 |
	       MTU_CRn_32BITS | MTU_CRn_ENA,
	       mtu0_base + MTU_CR(0));
	return 0;
}

void mtu_clockevent_reset(void)
{
	if (mtu_periodic) {

		/* Timer: configure load and background-load, and fire it up */
		writel(u8500_cycle, mtu0_base + MTU_LR(0));
		writel(u8500_cycle, mtu0_base + MTU_BGLR(0));

		writel(MTU_CRn_PERIODIC | MTU_CRn_PRESCALE_1 |
		       MTU_CRn_32BITS | MTU_CRn_ENA,
		       mtu0_base + MTU_CR(0));
		writel(1 << 0, mtu0_base + MTU_IMSC);
	} else {
		u8500_mtu_clkevt_next(u8500_cycle, NULL);
	}
}

/*
 * IRQ Handler for the timer 0 of the MTU block. The irq is not shared
 * as we are the only users of mtu0 by now.
 */
static irqreturn_t u8500_timer_interrupt(int irq, void *dev)
{
	struct clock_event_device *clkevt = dev;
	/* ack: "interrupt clear register" */
	writel(1 << 0, mtu0_base + MTU_ICR);

	clkevt->event_handler(clkevt);

	return IRQ_HANDLED;
}

/*
 * Added here as asm/smp.h is removed in v2.6.34 and
 * this funcitons is needed for current PM setup.
 */
#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
void smp_timer_broadcast(const struct cpumask *mask);
#endif

struct clock_event_device u8500_mtu_clkevt = {
	.name		= "mtu_0",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	/* Must be of higher rating the timer-rtt but lower than localtimers */
	.rating		= 310,
	.set_mode	= u8500_mtu_clkevt_mode,
	.set_next_event	= u8500_mtu_clkevt_next,
	.irq		= IRQ_MTU0,
#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
	.broadcast      = smp_timer_broadcast,
#endif
	.cpumask	= cpu_all_mask,
};

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static struct irqaction u8500_timer_irq = {
	.name		= "MTU Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= u8500_timer_interrupt,
	.dev_id		= &u8500_mtu_clkevt,
};

void __init mtu_timer_init(void)
{
	unsigned long rate;
	struct clk *clk0;

	clk0 = clk_get_sys("mtu0", NULL);
	BUG_ON(IS_ERR(clk0));

	rate = clk_get_rate(clk0);

	clk_enable(clk0);

	/*
	 * Set scale and timer for sched_clock
	 */
	setup_sched_clock(rate);
	u8500_cycle = (rate + HZ/2) / HZ;

	/* Save global pointer to mtu, used by functions above */
	if (cpu_is_u5500()) {
		mtu0_base = ioremap(U5500_MTU0_BASE, SZ_4K);
	} else if (cpu_is_u8500()) {
		mtu0_base = ioremap(U8500_MTU0_BASE, SZ_4K);
	} else {
		ux500_unknown_soc();
	}

	/* Restart clock source */
	mtu_clocksource_reset();

	/* Now the scheduling clock is ready */
	u8500_clksrc.read = u8500_read_timer;
	u8500_clksrc.mult = clocksource_hz2mult(rate, u8500_clksrc.shift);

	clocksource_register(&u8500_clksrc);

	/* Register irq and clockevents */
	u8500_mtu_clkevt.mult = div_sc(rate, NSEC_PER_SEC,
				       u8500_mtu_clkevt.shift);
	u8500_mtu_clkevt.max_delta_ns = clockevent_delta2ns(0xffffffff,
							    &u8500_mtu_clkevt);
	u8500_mtu_clkevt.min_delta_ns = clockevent_delta2ns(0xff,
							    &u8500_mtu_clkevt);

	setup_irq(IRQ_MTU0, &u8500_timer_irq);
	clockevents_register_device(&u8500_mtu_clkevt);
#ifdef ARCH_HAS_READ_CURRENT_TIMER
	set_delay_fn(mtu_timer_delay_loop);
#endif
}
