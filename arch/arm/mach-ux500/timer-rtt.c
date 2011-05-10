/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Rabin Vincent <rabin.vincent@stericsson.com>
 *
 * NOTE: This timer is optimized to be used from cpuidle only, so
 * if you enable this timer as broadcast timer, it won't work.
 *
 */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/clockchips.h>

#include <asm/smp.h>
#include <asm/mach/time.h>

#define RATE_32K	32768

#define RTC_IMSC	0x10
#define RTC_MIS		0x18
#define RTC_ICR		0x1C
#define RTC_TDR		0x20
#define	RTC_TLR1	0x24
#define RTC_TCR		0x28

#define RTC_TCR_RTTOS	(1 << 0)
#define RTC_TCR_RTTEN	(1 << 1)
#define RTC_TCR_RTTSS	(1 << 2)

#define RTC_IMSC_TIMSC	(1 << 1)
#define RTC_ICR_TIC	(1 << 1)
#define RTC_MIS_RTCTMIS	(1 << 1)

static void __iomem *rtc_base;

static void rtc_set_mode(enum clock_event_mode mode,
			       struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		pr_err("timer-rtt: periodic timer not supported\n");
	case CLOCK_EVT_MODE_ONESHOT:
		break;

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		/* Disable, self start and oneshot mode */
		writel(RTC_TCR_RTTSS | RTC_TCR_RTTOS, rtc_base + RTC_TCR);
		/*
		 * Here you should actually wait for 130 us before
		 * touching RTC_TCR again.
		 */
		break;

	case CLOCK_EVT_MODE_RESUME:
		break;

	}
}

static int rtc_set_event(unsigned long delta,
			       struct clock_event_device *dev)
{
	/*
	 * Here you must be sure that the timer is off or else
	 * you'll probably fail programming it.
	 */
	writel(delta, rtc_base + RTC_TLR1);

	/* Here you must be sure not to touch TCR for 130 us */

	return 0;
}

static irqreturn_t rtc_interrupt(int irq, void *dev)
{

	/* we make sure if this is our rtt isr */
	if (readl(rtc_base + RTC_MIS) & RTC_MIS_RTCTMIS) {
		writel(RTC_ICR_TIC, rtc_base + RTC_ICR);
		/* Here you should normally call the event handler */
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

/*
 * Added here as asm/smp.h is removed in v2.6.34 and
 * this funcitons is needed for current PM setup.
 */
#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
void smp_timer_broadcast(const struct cpumask *mask);
#endif

struct clock_event_device rtt_clkevt = {
	.name		= "rtc-rtt",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	/* This timer is not working except from cpuidle */
	.rating		= 0,
	.set_next_event	= rtc_set_event,
	.set_mode	= rtc_set_mode,
#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
	.broadcast      = smp_timer_broadcast,
#endif
	.cpumask	= cpu_all_mask,
};

static struct irqaction rtc_irq = {
	.name		= "RTC-RTT Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_SHARED,
	.handler	= rtc_interrupt,
	.irq		= IRQ_DB8500_RTC,
};

void rtc_rtt_timer_init(unsigned int cpu)
{

	if (cpu_is_u8500()) {
		rtc_base  = __io_address(U8500_RTC_BASE);
	} else if (cpu_is_u5500()) {
		rtc_base  = __io_address(U5500_RTC_BASE);
		rtt_clkevt.irq = IRQ_DB5500_RTC;
	} else {
		pr_err("timer-rtt: Unknown DB Asic!\n");
		return;
	}

	writel(RTC_TCR_RTTSS | RTC_TCR_RTTOS, rtc_base + RTC_TCR);
	writel(RTC_ICR_TIC, rtc_base + RTC_ICR);
	writel(RTC_IMSC_TIMSC, rtc_base + RTC_IMSC);

	rtt_clkevt.mult = div_sc(RATE_32K, NSEC_PER_SEC, rtt_clkevt.shift);
	rtt_clkevt.max_delta_ns = clockevent_delta2ns(0xffffffff, &rtt_clkevt);
	rtt_clkevt.min_delta_ns = clockevent_delta2ns(0xff, &rtt_clkevt);
	setup_irq(rtc_irq.irq, &rtc_irq);
	clockevents_register_device(&rtt_clkevt);
}
