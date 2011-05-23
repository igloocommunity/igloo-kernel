/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * U5500 PRCMU API.
 */
#ifndef __MACH_PRCMU_U5500_H
#define __MACH_PRCMU_U5500_H

/*
 * Clock identifiers.
 */
enum db5500_prcmu_clock {
	DB5500_PRCMU_SGACLK,
	DB5500_PRCMU_UARTCLK,
	DB5500_PRCMU_MSP02CLK,
	DB5500_PRCMU_I2CCLK,
	DB5500_PRCMU_SDMMCCLK,
	DB5500_PRCMU_PER1CLK,
	DB5500_PRCMU_PER2CLK,
	DB5500_PRCMU_PER3CLK,
	DB5500_PRCMU_PER5CLK,
	DB5500_PRCMU_PER6CLK,
	DB5500_PRCMU_PWMCLK,
	DB5500_PRCMU_IRDACLK,
	DB5500_PRCMU_IRRCCLK,
	DB5500_PRCMU_HDMICLK,
	DB5500_PRCMU_APEATCLK,
	DB5500_PRCMU_APETRACECLK,
	DB5500_PRCMU_MCDECLK,
	DB5500_PRCMU_DSIALTCLK,
	DB5500_PRCMU_DMACLK,
	DB5500_PRCMU_B2R2CLK,
	DB5500_PRCMU_TVCLK,
	DB5500_PRCMU_RNGCLK,
	DB5500_PRCMU_NUM_REG_CLOCKS,
	DB5500_PRCMU_SYSCLK = DB5500_PRCMU_NUM_REG_CLOCKS,
	DB5500_PRCMU_TIMCLK,
};

#ifdef CONFIG_UX500_SOC_DB5500

void db5500_prcmu_early_init(void);

int db5500_prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size);
int db5500_prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size);

int db5500_prcmu_request_clock(u8 clock, bool enable);

int prcmu_resetout(u8 resoutn, u8 state);

static inline int prcmu_set_power_state(u8 state, bool keep_ulp_clk,
	bool keep_ap_pll)
{
	return 0;
}

static inline int prcmu_set_epod(u16 epod_id, u8 epod_state)
{
	return 0;
}

static inline int prcmu_request_clock(u8 clock, bool enable)
{
	return db5500_prcmu_request_clock(clock, enable);
}

static inline void prcmu_system_reset(u16 reset_code) {}

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

static inline int prcmu_resetout(u8 resoutn, u8 state)
{
	return 0;
}

#endif /* CONFIG_UX500_SOC_DB5500 */

static inline int db5500_prcmu_config_abb_event_readout(u32 abb_events)
{
#ifdef CONFIG_MACH_U5500_SIMULATOR
	return 0;
#else
	return -1;
#endif
}

#endif /* __MACH_PRCMU_U5500_H */
