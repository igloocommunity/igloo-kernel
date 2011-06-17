/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * U5500 PRCMU API.
 */
#ifndef __MACH_PRCMU_U5500_H
#define __MACH_PRCMU_U5500_H

#ifdef CONFIG_UX500_SOC_DB5500

void db5500_prcmu_early_init(void);
int db5500_prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size);
int db5500_prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size);
int db5500_prcmu_request_clock(u8 clock, bool enable);
void db5500_prcmu_enable_wakeups(u32 wakeups);
int db5500_prcmu_set_epod(u16 epod_id, u8 epod_state);
int db5500_prcmu_set_display_clocks(void);
int db5500_prcmu_disable_dsipll(void);
int db5500_prcmu_enable_dsipll(void);
void db5500_prcmu_config_abb_event_readout(u32 abb_events);
void db5500_prcmu_get_abb_event_buffer(void __iomem **buf);
int prcmu_resetout(u8 resoutn, u8 state);
int db5500_prcmu_set_power_state(u8 state, bool keep_ulp_clk,
	bool keep_ap_pll);
int db5500_prcmu_config_esram0_deep_sleep(u8 state);

static inline void db5500_prcmu_system_reset(u16 reset_code) {}

#else /* !CONFIG_UX500_SOC_DB5500 */

static inline void db5500_prcmu_early_init(void)
{
}

static inline int db5500_prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size)
{
	return -ENOSYS;
}

static inline int db5500_prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size)
{
	return -ENOSYS;
}

static inline int db5500_prcmu_set_display_clocks(void)
{
	return 0;
}

static inline int db5500_prcmu_disable_dsipll(void)
{
	return 0;
}

static inline int db5500_prcmu_enable_dsipll(void)
{
	return 0;
}

static inline int db5500_prcmu_config_esram0_deep_sleep(u8 state)
{
	return 0;
}

static inline void db5500_prcmu_enable_wakeups(u32 wakeups) {}

static inline int prcmu_resetout(u8 resoutn, u8 state)
{
	return 0;
}

static inline int db5500_prcmu_set_epod(u16 epod_id, u8 epod_state)
{
	return 0;
}

static inline void db5500_prcmu_get_abb_event_buffer(void __iomem **buf) {}
static inline void db5500_prcmu_config_abb_event_readout(u32 abb_events) {}

static inline int db5500_prcmu_set_power_state(u8 state, bool keep_ulp_clk,
	bool keep_ap_pll)
{
	return 0;
}

#endif /* CONFIG_UX500_SOC_DB5500 */

#endif /* __MACH_PRCMU_U5500_H */
