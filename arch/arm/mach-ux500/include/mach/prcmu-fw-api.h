/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Kumar Sanghvi <kumar.sanghvi@stericsson.com>
 *
 * PRCMU f/w APIs
 */
#ifndef __MACH_PRCMU_FW_API_H
#define __MACH_PRCMU_FW_API_H

#include <linux/interrupt.h>
#include <linux/notifier.h>
#include "prcmu-fw-defs_v1.h"

/* PRCMU Wakeup defines */
enum prcmu_wakeup_index {
	PRCMU_WAKEUP_INDEX_RTC,
	PRCMU_WAKEUP_INDEX_RTT0,
	PRCMU_WAKEUP_INDEX_RTT1,
	PRCMU_WAKEUP_INDEX_HSI0,
	PRCMU_WAKEUP_INDEX_HSI1,
	PRCMU_WAKEUP_INDEX_USB,
	PRCMU_WAKEUP_INDEX_ABB,
	PRCMU_WAKEUP_INDEX_ABB_FIFO,
	PRCMU_WAKEUP_INDEX_ARM,
	NUM_PRCMU_WAKEUP_INDICES
};
#define PRCMU_WAKEUP(_name) (BIT(PRCMU_WAKEUP_INDEX_##_name))

/* PRCMU QoS APE OPP class */
#define PRCMU_QOS_APE_OPP 1
#define PRCMU_QOS_DDR_OPP 2
#define PRCMU_QOS_DEFAULT_VALUE -1

/**
 * enum hw_acc_dev - enum for hw accelerators
 * @HW_ACC_SVAMMDSP: for SVAMMDSP
 * @HW_ACC_SVAPIPE:  for SVAPIPE
 * @HW_ACC_SIAMMDSP: for SIAMMDSP
 * @HW_ACC_SIAPIPE: for SIAPIPE
 * @HW_ACC_SGA: for SGA
 * @HW_ACC_B2R2: for B2R2
 * @HW_ACC_MCDE: for MCDE
 * @HW_ACC_ESRAM1: for ESRAM1
 * @HW_ACC_ESRAM2: for ESRAM2
 * @HW_ACC_ESRAM3: for ESRAM3
 * @HW_ACC_ESRAM4: for ESRAM4
 * @NUM_HW_ACC: number of hardware accelerators
 *
 * Different hw accelerators which can be turned ON/
 * OFF or put into retention (MMDSPs and ESRAMs).
 * Used with EPOD API.
 *
 * NOTE! Deprecated, to be removed when all users switched over to use the
 * regulator API.
 */
enum hw_acc_dev{
	HW_ACC_SVAMMDSP,
	HW_ACC_SVAPIPE,
	HW_ACC_SIAMMDSP,
	HW_ACC_SIAPIPE,
	HW_ACC_SGA,
	HW_ACC_B2R2,
	HW_ACC_MCDE,
	HW_ACC_ESRAM1,
	HW_ACC_ESRAM2,
	HW_ACC_ESRAM3,
	HW_ACC_ESRAM4,
	NUM_HW_ACC
};

/*
 * Ids for all EPODs (power domains)
 * - EPOD_ID_SVAMMDSP: power domain for SVA MMDSP
 * - EPOD_ID_SVAPIPE: power domain for SVA pipe
 * - EPOD_ID_SIAMMDSP: power domain for SIA MMDSP
 * - EPOD_ID_SIAPIPE: power domain for SIA pipe
 * - EPOD_ID_SGA: power domain for SGA
 * - EPOD_ID_B2R2_MCDE: power domain for B2R2 and MCDE
 * - EPOD_ID_ESRAM12: power domain for ESRAM 1 and 2
 * - EPOD_ID_ESRAM34: power domain for ESRAM 3 and 4
 * - NUM_EPOD_ID: number of power domains
 */
#define EPOD_ID_SVAMMDSP	0
#define EPOD_ID_SVAPIPE		1
#define EPOD_ID_SIAMMDSP	2
#define EPOD_ID_SIAPIPE		3
#define EPOD_ID_SGA		4
#define EPOD_ID_B2R2_MCDE	5
#define EPOD_ID_ESRAM12		6
#define EPOD_ID_ESRAM34		7
#define NUM_EPOD_ID		8

/*
 * state definition for EPOD (power domain)
 * - EPOD_STATE_NO_CHANGE: The EPOD should remain unchanged
 * - EPOD_STATE_OFF: The EPOD is switched off
 * - EPOD_STATE_RAMRET: The EPOD is switched off with its internal RAM in
 *                         retention
 * - EPOD_STATE_ON_CLK_OFF: The EPOD is switched on, clock is still off
 * - EPOD_STATE_ON: Same as above, but with clock enabled
 */
#define EPOD_STATE_NO_CHANGE	0x00
#define EPOD_STATE_OFF		0x01
#define EPOD_STATE_RAMRET	0x02
#define EPOD_STATE_ON_CLK_OFF	0x03
#define EPOD_STATE_ON		0x04

/*
 * CLKOUT sources
 */
#define PRCMU_CLKSRC_CLK38M		0x00
#define PRCMU_CLKSRC_ACLK		0x01
#define PRCMU_CLKSRC_SYSCLK		0x02
#define PRCMU_CLKSRC_LCDCLK		0x03
#define PRCMU_CLKSRC_SDMMCCLK		0x04
#define PRCMU_CLKSRC_TVCLK		0x05
#define PRCMU_CLKSRC_TIMCLK		0x06
#define PRCMU_CLKSRC_CLK009		0x07
/* These are only valid for CLKOUT1: */
#define PRCMU_CLKSRC_SIAMMDSPCLK	0x40
#define PRCMU_CLKSRC_I2CCLK		0x41
#define PRCMU_CLKSRC_MSP02CLK		0x42
#define PRCMU_CLKSRC_ARMPLL_OBSCLK	0x43
#define PRCMU_CLKSRC_HSIRXCLK		0x44
#define PRCMU_CLKSRC_HSITXCLK		0x45
#define PRCMU_CLKSRC_ARMCLKFIX		0x46
#define PRCMU_CLKSRC_HDMICLK		0x47

/*
 * Definitions for autonomous power management configuration.
 */

#define PRCMU_AUTO_PM_OFF 0
#define PRCMU_AUTO_PM_ON 1

#define PRCMU_AUTO_PM_POWER_ON_HSEM BIT(0)
#define PRCMU_AUTO_PM_POWER_ON_ABB_FIFO_IT BIT(1)

enum prcmu_auto_pm_policy {
	PRCMU_AUTO_PM_POLICY_NO_CHANGE,
	PRCMU_AUTO_PM_POLICY_DSP_OFF_HWP_OFF,
	PRCMU_AUTO_PM_POLICY_DSP_OFF_RAMRET_HWP_OFF,
	PRCMU_AUTO_PM_POLICY_DSP_CLK_OFF_HWP_OFF,
	PRCMU_AUTO_PM_POLICY_DSP_CLK_OFF_HWP_CLK_OFF,
};

/**
 * struct prcmu_auto_pm_config - Autonomous power management configuration.
 * @sia_auto_pm_enable: SIA autonomous pm enable. (PRCMU_AUTO_PM_{OFF,ON})
 * @sia_power_on:       SIA power ON enable. (PRCMU_AUTO_PM_POWER_ON_* bitmask)
 * @sia_policy:         SIA power policy. (enum prcmu_auto_pm_policy)
 * @sva_auto_pm_enable: SVA autonomous pm enable. (PRCMU_AUTO_PM_{OFF,ON})
 * @sva_power_on:       SVA power ON enable. (PRCMU_AUTO_PM_POWER_ON_* bitmask)
 * @sva_policy:         SVA power policy. (enum prcmu_auto_pm_policy)
 */
struct prcmu_auto_pm_config {
	u8 sia_auto_pm_enable;
	u8 sia_power_on;
	u8 sia_policy;
	u8 sva_auto_pm_enable;
	u8 sva_power_on;
	u8 sva_policy;
};

/**
 * enum ddr_opp - DDR OPP states definition
 * @DDR_100_OPP: The new DDR operating point is ddr100opp
 * @DDR_50_OPP: The new DDR operating point is ddr50opp
 * @DDR_25_OPP: The new DDR operating point is ddr25opp
 */
enum ddr_opp {
	DDR_100_OPP = 0x00,
	DDR_50_OPP = 0x01,
	DDR_25_OPP = 0x02,
};

/*
 * Clock identifiers.
 */
enum prcmu_clock {
	PRCMU_SGACLK,
	PRCMU_UARTCLK,
	PRCMU_MSP02CLK,
	PRCMU_MSP1CLK,
	PRCMU_I2CCLK,
	PRCMU_SDMMCCLK,
	PRCMU_SLIMCLK,
	PRCMU_PER1CLK,
	PRCMU_PER2CLK,
	PRCMU_PER3CLK,
	PRCMU_PER5CLK,
	PRCMU_PER6CLK,
	PRCMU_PER7CLK,
	PRCMU_LCDCLK,
	PRCMU_BMLCLK,
	PRCMU_HSITXCLK,
	PRCMU_HSIRXCLK,
	PRCMU_HDMICLK,
	PRCMU_APEATCLK,
	PRCMU_APETRACECLK,
	PRCMU_MCDECLK,
	PRCMU_IPI2CCLK,
	PRCMU_DSIALTCLK,
	PRCMU_DMACLK,
	PRCMU_B2R2CLK,
	PRCMU_TVCLK,
	PRCMU_SSPCLK,
	PRCMU_RNGCLK,
	PRCMU_UICCCLK,
	PRCMU_NUM_REG_CLOCKS,
	PRCMU_SYSCLK = PRCMU_NUM_REG_CLOCKS,
	PRCMU_TIMCLK,
};

/*
 * Definitions for controlling ESRAM0 in deep sleep.
 */
#define ESRAM0_DEEP_SLEEP_STATE_OFF 1
#define ESRAM0_DEEP_SLEEP_STATE_RET 2

#if defined(CONFIG_UX500_SOC_DB8500) || defined(CONFIG_UX500_SOC_DB5500)
void __init prcmu_early_init(void);
int prcmu_set_display_clocks(void);
int prcmu_disable_dsipll(void);
int prcmu_enable_dsipll(void);
#else
static inline void __init prcmu_early_init(void) {}
#endif

#ifdef CONFIG_UX500_SOC_DB8500

int prcmu_set_rc_a2p(enum romcode_write);
enum romcode_read prcmu_get_rc_p2a(void);
enum ap_pwrst prcmu_get_xp70_current_state(void);
int prcmu_set_power_state(u8 state, bool keep_ulp_clk, bool keep_ap_pll);

void prcmu_enable_wakeups(u32 wakeups);
static inline void prcmu_disable_wakeups(void)
{
	prcmu_enable_wakeups(0);
}

void prcmu_config_abb_event_readout(u32 abb_events);
void prcmu_get_abb_event_buffer(void __iomem **buf);
int prcmu_set_arm_opp(u8 opp);
int prcmu_get_arm_opp(void);
bool prcmu_has_arm_maxopp(void);
bool prcmu_is_u8400(void);
int prcmu_set_ape_opp(u8 opp);
int prcmu_get_ape_opp(void);
int prcmu_request_ape_opp_100_voltage(bool enable);
int prcmu_release_usb_wakeup_state(void);
int prcmu_set_ddr_opp(u8 opp);
int prcmu_get_ddr_opp(void);
unsigned long prcmu_qos_get_cpufreq_opp_delay(void);
void prcmu_qos_set_cpufreq_opp_delay(unsigned long);
/* NOTE! Use regulator framework instead */
int prcmu_set_hwacc(u16 hw_acc_dev, u8 state);
int prcmu_set_epod(u16 epod_id, u8 epod_state);
void prcmu_configure_auto_pm(struct prcmu_auto_pm_config *sleep,
	struct prcmu_auto_pm_config *idle);
bool prcmu_is_auto_pm_enabled(void);

int prcmu_config_clkout(u8 clkout, u8 source, u8 div);
int prcmu_request_clock(u8 clock, bool enable);
int prcmu_set_clock_divider(u8 clock, u8 divider);
int prcmu_config_esram0_deep_sleep(u8 state);
int prcmu_config_hotdog(u8 threshold);
int prcmu_config_hotmon(u8 low, u8 high);
int prcmu_start_temp_sense(u16 cycles32k);
int prcmu_stop_temp_sense(void);
int prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size);
int prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size);

void prcmu_ac_wake_req(void);
void prcmu_ac_sleep_req(void);
void prcmu_system_reset(void);
void prcmu_modem_reset(void);
bool prcmu_is_ac_wake_requested(void);
void prcmu_enable_spi2(void);
void prcmu_disable_spi2(void);

#else /* !CONFIG_UX500_SOC_DB8500 */

static inline int prcmu_set_rc_a2p(enum romcode_write code)
{
	return 0;
}

static inline enum romcode_read prcmu_get_rc_p2a(void)
{
	return INIT;
}

static inline enum ap_pwrst prcmu_get_xp70_current_state(void)
{
	return AP_EXECUTE;
}

static inline int prcmu_set_power_state(u8 state, bool keep_ulp_clk,
	bool keep_ap_pll)
{
	return 0;
}

static inline void prcmu_enable_wakeups(u32 wakeups) {}

static inline void prcmu_disable_wakeups(void) {}

static inline void prcmu_config_abb_event_readout(u32 abb_events) {}

static inline int prcmu_set_arm_opp(u8 opp)
{
	return 0;
}

static inline int prcmu_get_arm_opp(void)
{
	return ARM_100_OPP;
}

static bool prcmu_has_arm_maxopp(void)
{
	return false;
}

static bool prcmu_is_u8400(void)
{
	return false;
}

static inline int prcmu_set_ape_opp(u8 opp)
{
	return 0;
}

static inline int prcmu_get_ape_opp(void)
{
	return APE_100_OPP;
}

static inline int prcmu_request_ape_opp_100_voltage(bool enable)
{
	return 0;
}

static inline int prcmu_release_usb_wakeup_state(void)
{
	return 0;
}

static inline int prcmu_set_ddr_opp(u8 opp)
{
	return 0;
}

static inline int prcmu_get_ddr_opp(void)
{
	return DDR_100_OPP;
}

static inline unsigned long prcmu_qos_get_cpufreq_opp_delay(void)
{
	return 0;
}

static inline void prcmu_qos_set_cpufreq_opp_delay(unsigned long n) {}

static inline int prcmu_set_hwacc(u16 hw_acc_dev, u8 state)
{
	return 0;
}

static inline void prcmu_configure_auto_pm(struct prcmu_auto_pm_config *sleep,
	struct prcmu_auto_pm_config *idle)
{
}

static inline bool prcmu_is_auto_pm_enabled(void)
{
	return false;
}

static inline int prcmu_config_clkout(u8 clkout, u8 source, u8 div)
{
	return 0;
}

static inline int prcmu_request_clock(u8 clock, bool enable)
{
	return 0;
}

static inline int prcmu_set_clock_divider(u8 clock, u8 divider)
{
	return 0;
}

int prcmu_config_esram0_deep_sleep(u8 state)
{
	return 0;
}

static inline int prcmu_config_hotdog(u8 threshold)
{
	return 0;
}

static inline int prcmu_config_hotmon(u8 low, u8 high)
{
	return 0;
}

static inline int prcmu_start_temp_sense(u16 cycles32k)
{
	return 0;
}

static inline int prcmu_stop_temp_sense(void)
{
	return 0;
}

static inline int prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size)
{
	return -ENOSYS;
}

static inline int prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size)
{
	return -ENOSYS;
}

static inline void prcmu_ac_wake_req(void) {}

static inline void prcmu_ac_sleep_req(void) {}

static inline void prcmu_system_reset(void) {}

static inline void prcmu_modem_reset(void) {}

static inline bool prcmu_is_ac_wake_requested(void)
{
	return false;
}

#ifndef CONFIG_UX500_SOC_DB5500
static inline int prcmu_set_display_clocks(void)
{
	return 0;
}

static inline int prcmu_disable_dsipll(void)
{
	return 0;
}

static inline int prcmu_enable_dsipll(void)
{
	return 0;
}
#endif

static inline int prcmu_enable_spi2(void)
{
	return 0;
}

static inline int prcmu_disable_spi2(void)
{
	return 0;
}

#endif /* !CONFIG_UX500_SOC_DB5500 */

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

#endif /* __MACH_PRCMU_FW_API_V1_H */
