/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * U5500 PRCMU API.
 */
#ifndef __MACH_PRCMU_U5500_H
#define __MACH_PRCMU_U5500_H

#ifdef CONFIG_U5500_PRCMU

int db5500_prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size);
int db5500_prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size);

int db5500_prcmu_request_clock(u8 clock, bool enable);

static inline unsigned long prcmu_clock_rate(u8 clock)
{
	return 0;
}

static inline long prcmu_round_clock_rate(u8 clock, unsigned long rate)
{
	return 0;
}

static inline int prcmu_set_clock_rate(u8 clock, unsigned long rate)
{
	return 0;
}

int prcmu_resetout(u8 resoutn, u8 state);

unsigned int prcmu_get_ddr_freq(void);
int prcmu_get_hotdog(void);

#else /* !CONFIG_U5500_PRCMU */

static inline int db5500_prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size)
{
	return -ENOSYS;
}

static inline int db5500_prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size)
{
	return -ENOSYS;
}

static inline int prcmu_resetout(u8 resoutn, u8 state)
{
	return 0;
}

static inline unsigned int prcmu_get_ddr_freq(void)
{
	return 0;
}

static inline int prcmu_get_hotdog(void)
{
	return -ENOSYS;
}

#endif /* CONFIG_U5500_PRCMU */

#endif /* __MACH_PRCMU_U5500_H */
