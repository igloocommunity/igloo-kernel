/*
 *  linux/arch/arm/kernel/smp_twd.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/jiffies.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/cpufreq.h>

#include <asm/smp_twd.h>
#include <asm/hardware/gic.h>

/* set up by the platform code */
void __iomem *twd_base;

static unsigned long twd_timer_rate;
static unsigned long twd_periphclk_prescaler;
static unsigned long twd_target_rate;

static DEFINE_PER_CPU(unsigned long, twd_ctrl);
static DEFINE_PER_CPU(unsigned long, twd_load);

static void twd_set_mode(enum clock_event_mode mode,
			struct clock_event_device *clk)
{
	unsigned long ctrl;
	int this_cpu = smp_processor_id();

	__raw_writel(per_cpu(twd_load, this_cpu),
				 twd_base + TWD_TIMER_LOAD);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		/* timer load already set up */
		ctrl = TWD_TIMER_CONTROL_ENABLE | TWD_TIMER_CONTROL_IT_ENABLE
			| TWD_TIMER_CONTROL_PERIODIC;
		__raw_writel(twd_timer_rate / HZ, twd_base + TWD_TIMER_LOAD);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		ctrl = TWD_TIMER_CONTROL_IT_ENABLE | TWD_TIMER_CONTROL_ONESHOT;
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		ctrl = 0;
	}

	ctrl |= per_cpu(twd_ctrl, this_cpu) & TWD_TIMER_CONTROL_PRESCALE_MASK;

	__raw_writel(ctrl, twd_base + TWD_TIMER_CONTROL);
	per_cpu(twd_ctrl, this_cpu) = ctrl;
}

static int twd_set_next_event(unsigned long evt,
			struct clock_event_device *unused)
{
	int this_cpu = smp_processor_id();

	per_cpu(twd_ctrl, this_cpu) |= TWD_TIMER_CONTROL_ENABLE;

	__raw_writel(evt, twd_base + TWD_TIMER_COUNTER);
	__raw_writel(per_cpu(twd_ctrl, this_cpu),
				 twd_base + TWD_TIMER_CONTROL);

	return 0;
}

/*
 * local_timer_ack: checks for a local timer interrupt.
 *
 * If a local timer interrupt has occurred, acknowledge and return 1.
 * Otherwise, return 0.
 */
int twd_timer_ack(void)
{
	if (__raw_readl(twd_base + TWD_TIMER_INTSTAT)) {
		__raw_writel(1, twd_base + TWD_TIMER_INTSTAT);
		return 1;
	}

	return 0;
}

/*
 * must be called with interrupts disabled and on the cpu that is being changed
 */
static void twd_update_cpu_frequency(unsigned long new_rate)
{
	int prescaler;
	int this_cpu = smp_processor_id();

	BUG_ON(twd_periphclk_prescaler == 0 || twd_target_rate == 0);

	twd_timer_rate = new_rate / twd_periphclk_prescaler;

	prescaler = DIV_ROUND_UP(twd_timer_rate, twd_target_rate);
	prescaler = clamp(prescaler - 1, 0, 0xFF);

	per_cpu(twd_ctrl, this_cpu) &= ~TWD_TIMER_CONTROL_PRESCALE_MASK;
	per_cpu(twd_ctrl, this_cpu) |= prescaler << 8;
	__raw_writel(per_cpu(twd_ctrl, this_cpu),
				 twd_base + TWD_TIMER_CONTROL);
}

static void twd_update_cpu_frequency_on_cpu(void *data)
{
	struct cpufreq_freqs *freq = data;

	twd_update_cpu_frequency(freq->new * 1000);
}

static int twd_cpufreq_notifier(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct cpufreq_freqs *freq = data;

	if (event == CPUFREQ_RESUMECHANGE ||
	    (event == CPUFREQ_PRECHANGE && freq->new > freq->old) ||
	    (event == CPUFREQ_POSTCHANGE && freq->new < freq->old))
		smp_call_function_single(freq->cpu,
			twd_update_cpu_frequency_on_cpu, freq, 1);

	return 0;
}

static struct notifier_block twd_cpufreq_notifier_block = {
	.notifier_call = twd_cpufreq_notifier,
};

static void __cpuinit twd_calibrate_rate(void)
{
	unsigned long count;
	u64 waitjiffies;

	/*
	 * If this is the first time round, we need to work out how fast
	 * the timer ticks
	 */
	if (twd_timer_rate == 0) {
		printk(KERN_INFO "Calibrating local timer... ");

		/* Wait for a tick to start */
		waitjiffies = get_jiffies_64() + 1;

		while (get_jiffies_64() < waitjiffies)
			udelay(10);

		/* OK, now the tick has started, let's get the timer going */
		waitjiffies += 5;

				 /* enable, no interrupt or reload */
		__raw_writel(TWD_TIMER_CONTROL_ENABLE,
					 twd_base + TWD_TIMER_CONTROL);

		per_cpu(twd_ctrl, smp_processor_id()) =
			TWD_TIMER_CONTROL_ENABLE;

				 /* maximum value */
		__raw_writel(0xFFFFFFFFU, twd_base + TWD_TIMER_COUNTER);

		while (get_jiffies_64() < waitjiffies)
			udelay(10);

		count = __raw_readl(twd_base + TWD_TIMER_COUNTER);

		twd_timer_rate = (0xFFFFFFFFU - count) * (HZ / 5);

		printk("%lu.%02luMHz.\n", twd_timer_rate / 1000000,
			(twd_timer_rate / 10000) % 100);
	}
}

/*
 * Setup the local clock events for a CPU.
 */
static void __cpuinit __twd_timer_setup(struct clock_event_device *clk,
	unsigned long target_rate, unsigned int periphclk_prescaler)
{
	unsigned long cpu_rate;
	unsigned long twd_tick_rate;

	twd_calibrate_rate();

	if (target_rate && periphclk_prescaler) {
		cpu_rate = twd_timer_rate * periphclk_prescaler;
		twd_target_rate = target_rate;
		twd_periphclk_prescaler = periphclk_prescaler;
		twd_update_cpu_frequency(cpu_rate);
		twd_tick_rate = twd_target_rate;
	} else {
		twd_tick_rate = twd_timer_rate;
	}

	per_cpu(twd_load, smp_processor_id()) = twd_tick_rate / HZ;

	clk->name = "local_timer";
#if defined(CONFIG_GENERIC_CLOCKEVENTS_BROADCAST) && \
	defined(CONFIG_LOCAL_TIMERS)
	clk->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT |
			CLOCK_EVT_FEAT_C3STOP;
#else
	clk->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
#endif
	clk->rating = 350;
	clk->set_mode = twd_set_mode;
	clk->set_next_event = twd_set_next_event;
	clk->shift = 20;
	clk->mult = div_sc(twd_tick_rate, NSEC_PER_SEC, clk->shift);
	clk->max_delta_ns = clockevent_delta2ns(0xffffffff, clk);
	clk->min_delta_ns = clockevent_delta2ns(0xf, clk);

	/* Make sure our local interrupt controller has this enabled */
	gic_enable_ppi(clk->irq);

	clockevents_register_device(clk);
}

void __cpuinit twd_timer_setup_scalable(struct clock_event_device *clk,
	unsigned long target_rate, unsigned int periphclk_prescaler)
{
	__twd_timer_setup(clk, target_rate, periphclk_prescaler);
}

void __cpuinit twd_timer_setup(struct clock_event_device *clk)
{
	__twd_timer_setup(clk, 0, 0);
}

static int twd_timer_setup_cpufreq(void)
{
	if (twd_periphclk_prescaler)
		cpufreq_register_notifier(&twd_cpufreq_notifier_block,
			CPUFREQ_TRANSITION_NOTIFIER);

	return 0;
}
arch_initcall(twd_timer_setup_cpufreq);

#ifdef CONFIG_HOTPLUG_CPU
/*
 * take a local timer down
 */
void twd_timer_stop(void)
{
	int this_cpu = smp_processor_id();
	per_cpu(twd_ctrl, this_cpu) &= ~(TWD_TIMER_CONTROL_ENABLE |
									 TWD_TIMER_CONTROL_IT_ENABLE);
	__raw_writel(per_cpu(twd_ctrl, this_cpu),
				 twd_base + TWD_TIMER_CONTROL);

}
#endif
