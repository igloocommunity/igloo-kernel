/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Rabin Vincent <rabin.vincent@stericsson.com>
 *
 */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/clockchips.h>
#include <linux/delay.h>

#include <asm/smp.h>
#include <asm/mach/time.h>

#define RATE_32K	32768
#define LATCH_32K	((RATE_32K + HZ/2) / HZ)

#define WRITE_DELAY 130 /* in us */
#define WRITE_DELAY_32KHZ_TICKS ((WRITE_DELAY * RATE_32K) / 1000000)

#define RTT_IMSC	0x04
#define RTT_ICR		0x10
#define RTT_DR		0x14
#define RTT_LR		0x18
#define RTT_CR		0x1C

#define RTT_CR_RTTEN	(1 << 1)
#define RTT_CR_RTTOS	(1 << 0)

#define RTC_IMSC	0x10
#define RTC_RIS		0x14
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

static void __iomem *rtc_base;
static void __iomem *rtt0_base;

static void rtc_writel(unsigned long val, unsigned long addr)
{
	writel(val, rtc_base + addr);
}

static unsigned long rtc_readl(unsigned long addr)
{
	return readl(rtc_base + addr);
}

static void rtc_set_mode(enum clock_event_mode mode,
			 struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		rtc_writel(RTC_TCR_RTTSS, RTC_TCR);
		rtc_writel(LATCH_32K, RTC_TLR1);
		break;

	case CLOCK_EVT_MODE_ONESHOT:
		rtc_writel(0, RTC_TCR);
		break;

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		rtc_writel(0, RTC_TCR);
		break;

	case CLOCK_EVT_MODE_RESUME:
		break;

	}
}

static int rtc_set_event(unsigned long delta,
			 struct clock_event_device *dev)
{

	rtc_writel(RTC_TCR_RTTOS, RTC_TCR);
	udelay(WRITE_DELAY);

	/*
	 * Compensate for the time that it takes to start the
	 * timer
	 */
	if (delta > WRITE_DELAY_32KHZ_TICKS * 2)
		delta -= WRITE_DELAY_32KHZ_TICKS * 2;
	else
		delta = 1;
	rtc_writel(delta, RTC_TLR1);
	udelay(WRITE_DELAY);

	rtc_writel(RTC_TCR_RTTOS | RTC_TCR_RTTEN, RTC_TCR);
	udelay(WRITE_DELAY);

	return 0;
}

static irqreturn_t rtc_interrupt(int irq, void *dev)
{
	struct clock_event_device *clkevt = dev;

	/* we make sure if this is our rtt isr */
	if (rtc_readl(RTC_MIS) & 0x2) {
		rtc_writel(RTC_ICR_TIC, RTC_ICR);
		clkevt->event_handler(clkevt);
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

static struct clock_event_device rtc_dev = {
	.name		= "rtc-rtt",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	.rating		= 300,
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
	.dev_id		= &rtc_dev,
};

void rtc_rtt_timer_init(unsigned int cpu)
{
	int irq;

	if (cpu_is_u8500()) {
		rtc_base  = __io_address(U8500_RTC_BASE);
		rtt0_base = __io_address(U8500_RTT0_BASE);
		irq = IRQ_DB8500_RTC;
	} else if (cpu_is_u5500()) {
		rtc_base  = __io_address(U5500_RTC_BASE);
		rtt0_base = __io_address(U5500_RTT0_BASE);
		irq = IRQ_DB5500_RTC;
	} else {
		pr_err("timer-rtt: Unknown DB Asic!\n");
		return;
	}

	rtc_dev.irq = irq;

	rtc_writel(0, RTC_TCR);
	rtc_writel(RTC_ICR_TIC, RTC_ICR);
	rtc_writel(RTC_IMSC_TIMSC, RTC_IMSC);

	rtc_dev.mult = div_sc(RATE_32K, NSEC_PER_SEC, rtc_dev.shift);
	rtc_dev.max_delta_ns = clockevent_delta2ns(0xffffffff, &rtc_dev);
	rtc_dev.min_delta_ns = clockevent_delta2ns(0xff, &rtc_dev);

	setup_irq(irq, &rtc_irq);
	clockevents_register_device(&rtc_dev);
}

int rtc_rtt_adjust_next_wakeup(int delta_in_us)
{
	int delta_ticks;
	u64 temp;
	u64 remainder;
	u32 val;

	/* Convert us to 32768 hz ticks */
	temp = ((u64)abs(delta_in_us)) * RATE_32K;
	remainder = do_div(temp, 1000000);

	if (delta_in_us < 0) {
		delta_ticks = - ((int) temp);
		/* Round upwards, since better to wake a little early */
		if ( remainder >= (1000000 / 2))
			delta_ticks--;
	} else {
		delta_ticks = (int) temp;
	}

	val = readl(rtc_base + RTC_TDR);

	/*
	 * Make sure that if we want to wake up earlier that it is
	 * possible.
	 */
	if ((int)val < -delta_ticks)
		return -EINVAL;

	/* Make sure we can wake up as late as requested */
	if (((u64)val + (u64)delta_ticks) > UINT_MAX)
		return -EINVAL;

	return rtc_set_event(val + delta_ticks, &rtc_dev);
}
