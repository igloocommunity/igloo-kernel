/*
 * Copyright (C) ST Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * PRCMU QoS
 */
#ifndef __MACH_PRCMU_QOS_H
#define __MACH_PRCMU_QOS_H

#include <linux/notifier.h>

/* PRCMU QoS APE OPP class */
#define PRCMU_QOS_APE_OPP 1
#define PRCMU_QOS_DDR_OPP 2
#define PRCMU_QOS_ARM_OPP 3
#define PRCMU_QOS_DEFAULT_VALUE -1

#if defined(CONFIG_U8500_PRCMU) || defined(CONFIG_U5500_PRCMU)

unsigned long prcmu_qos_get_cpufreq_opp_delay(void);
void prcmu_qos_set_cpufreq_opp_delay(unsigned long);
void prcmu_qos_force_opp(int, s32);
void prcmu_qos_voice_call_override(bool enable);

#else /* !CONFIG_U8500_PRCMU */

static inline unsigned long prcmu_qos_get_cpufreq_opp_delay(void)
{
	return 0;
}

static inline void prcmu_qos_set_cpufreq_opp_delay(unsigned long n) {}

static inline void prcmu_qos_force_opp(int prcmu_qos_class, s32 i) {}

static inline void prcmu_qos_voice_call_override(bool enable) {}

#endif /* CONFIG_U8500_PRCMU */

#ifdef CONFIG_UX500_PRCMU_QOS_POWER

int prcmu_qos_requirement(int pm_qos_class);
int prcmu_qos_add_requirement(int pm_qos_class, char *name, s32 value);
int prcmu_qos_update_requirement(int pm_qos_class, char *name, s32 new_value);
void prcmu_qos_remove_requirement(int pm_qos_class, char *name);
int prcmu_qos_add_notifier(int prcmu_qos_class,
			   struct notifier_block *notifier);
int prcmu_qos_remove_notifier(int prcmu_qos_class,
			      struct notifier_block *notifier);

#else

static inline int prcmu_qos_requirement(int prcmu_qos_class)
{
	return 0;
}

static inline int prcmu_qos_add_requirement(int prcmu_qos_class,
					    char *name, s32 value)
{
	return 0;
}

static inline int prcmu_qos_update_requirement(int prcmu_qos_class,
					       char *name, s32 new_value)
{
	return 0;
}

static inline void prcmu_qos_remove_requirement(int prcmu_qos_class, char *name)
{
}

static inline int prcmu_qos_add_notifier(int prcmu_qos_class,
					 struct notifier_block *notifier)
{
	return 0;
}
static inline int prcmu_qos_remove_notifier(int prcmu_qos_class,
					    struct notifier_block *notifier)
{
	return 0;
}

#endif

#endif /* __MACH_PRCMU_QOS_H */
