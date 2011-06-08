/*
 * Copyright (C) ST-Ericsson SA 2010
 * License terms: GNU General Public License v2
 * AB3100 core access functions
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 *
 * ABX500 core access functions.
 * The abx500 interface is used for the Analog Baseband chip
 * ab3100, ab3550, ab5500 and possibly comming. It is not used for
 * ab4500 and ab8500 since they are another family of chip.
 *
 * Author: Mattias Wallin <mattias.wallin@stericsson.com>
 * Author: Mattias Nilsson <mattias.i.nilsson@stericsson.com>
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com>
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>
 */

#include <linux/device.h>
#include <linux/regulator/machine.h>

#ifndef MFD_ABX500_H
#define MFD_ABX500_H

#define AB3100_P1A	0xc0
#define AB3100_P1B	0xc1
#define AB3100_P1C	0xc2
#define AB3100_P1D	0xc3
#define AB3100_P1E	0xc4
#define AB3100_P1F	0xc5
#define AB3100_P1G	0xc6
#define AB3100_R2A	0xc7
#define AB3100_R2B	0xc8
#define AB3550_P1A	0x10
#define AB5500_1_0	0x20
#define AB5500_1_1	0x21
#define AB5500_2_0	0x24

/* AB8500 CIDs*/
#define AB8500_CUTEARLY	0x00
#define AB8500_CUT1P0	0x10
#define AB8500_CUT1P1	0x11
#define AB8500_CUT2P0	0x20
#define AB8500_CUT3P0	0x30

/* AB8500 CIDs*/
#define AB8500_CUTEARLY	0x00
#define AB8500_CUT1P0	0x10
#define AB8500_CUT1P1	0x11
#define AB8500_CUT2P0	0x20
#define AB8500_CUT3P0	0x30

/*
 * AB3100, EVENTA1, A2 and A3 event register flags
 * these are catenated into a single 32-bit flag in the code
 * for event notification broadcasts.
 */
#define AB3100_EVENTA1_ONSWA				(0x01<<16)
#define AB3100_EVENTA1_ONSWB				(0x02<<16)
#define AB3100_EVENTA1_ONSWC				(0x04<<16)
#define AB3100_EVENTA1_DCIO				(0x08<<16)
#define AB3100_EVENTA1_OVER_TEMP			(0x10<<16)
#define AB3100_EVENTA1_SIM_OFF				(0x20<<16)
#define AB3100_EVENTA1_VBUS				(0x40<<16)
#define AB3100_EVENTA1_VSET_USB				(0x80<<16)

#define AB3100_EVENTA2_READY_TX				(0x01<<8)
#define AB3100_EVENTA2_READY_RX				(0x02<<8)
#define AB3100_EVENTA2_OVERRUN_ERROR			(0x04<<8)
#define AB3100_EVENTA2_FRAMING_ERROR			(0x08<<8)
#define AB3100_EVENTA2_CHARG_OVERCURRENT		(0x10<<8)
#define AB3100_EVENTA2_MIDR				(0x20<<8)
#define AB3100_EVENTA2_BATTERY_REM			(0x40<<8)
#define AB3100_EVENTA2_ALARM				(0x80<<8)

#define AB3100_EVENTA3_ADC_TRIG5			(0x01)
#define AB3100_EVENTA3_ADC_TRIG4			(0x02)
#define AB3100_EVENTA3_ADC_TRIG3			(0x04)
#define AB3100_EVENTA3_ADC_TRIG2			(0x08)
#define AB3100_EVENTA3_ADC_TRIGVBAT			(0x10)
#define AB3100_EVENTA3_ADC_TRIGVTX			(0x20)
#define AB3100_EVENTA3_ADC_TRIG1			(0x40)
#define AB3100_EVENTA3_ADC_TRIG0			(0x80)

/* AB3100, STR register flags */
#define AB3100_STR_ONSWA				(0x01)
#define AB3100_STR_ONSWB				(0x02)
#define AB3100_STR_ONSWC				(0x04)
#define AB3100_STR_DCIO					(0x08)
#define AB3100_STR_BOOT_MODE				(0x10)
#define AB3100_STR_SIM_OFF				(0x20)
#define AB3100_STR_BATT_REMOVAL				(0x40)
#define AB3100_STR_VBUS					(0x80)

/*
 * AB3100 contains 8 regulators, one external regulator controller
 * and a buck converter, further the LDO E and buck converter can
 * have separate settings if they are in sleep mode, this is
 * modeled as a separate regulator.
 */
#define AB3100_NUM_REGULATORS				10

/**
 * struct ab3100
 * @access_mutex: lock out concurrent accesses to the AB3100 registers
 * @dev: pointer to the containing device
 * @i2c_client: I2C client for this chip
 * @testreg_client: secondary client for test registers
 * @chip_name: name of this chip variant
 * @chip_id: 8 bit chip ID for this chip variant
 * @event_subscribers: event subscribers are listed here
 * @startup_events: a copy of the first reading of the event registers
 * @startup_events_read: whether the first events have been read
 *
 * This struct is PRIVATE and devices using it should NOT
 * access ANY fields. It is used as a token for calling the
 * AB3100 functions.
 */
struct ab3100 {
	struct mutex access_mutex;
	struct device *dev;
	struct i2c_client *i2c_client;
	struct i2c_client *testreg_client;
	char chip_name[32];
	u8 chip_id;
	struct blocking_notifier_head event_subscribers;
	u8 startup_events[3];
	bool startup_events_read;
};

/**
 * struct ab3100_platform_data
 * Data supplied to initialize board connections to the AB3100
 * @reg_constraints: regulator constraints for target board
 *     the order of these constraints are: LDO A, C, D, E,
 *     F, G, H, K, EXT and BUCK.
 * @reg_initvals: initial values for the regulator registers
 *     plus two sleep settings for LDO E and the BUCK converter.
 *     exactly AB3100_NUM_REGULATORS+2 values must be sent in.
 *     Order: LDO A, C, E, E sleep, F, G, H, K, EXT, BUCK,
 *     BUCK sleep, LDO D. (LDO D need to be initialized last.)
 * @external_voltage: voltage level of the external regulator.
 */
struct ab3100_platform_data {
	struct regulator_init_data reg_constraints[AB3100_NUM_REGULATORS];
	u8 reg_initvals[AB3100_NUM_REGULATORS+2];
	int external_voltage;
};

int ab3100_event_register(struct ab3100 *ab3100,
			  struct notifier_block *nb);
int ab3100_event_unregister(struct ab3100 *ab3100,
			    struct notifier_block *nb);

/* AB3550, STR register flags */
#define AB3550_STR_ONSWA				(0x01)
#define AB3550_STR_ONSWB				(0x02)
#define AB3550_STR_ONSWC				(0x04)
#define AB3550_STR_DCIO					(0x08)
#define AB3550_STR_BOOT_MODE				(0x10)
#define AB3550_STR_SIM_OFF				(0x20)
#define AB3550_STR_BATT_REMOVAL				(0x40)
#define AB3550_STR_VBUS					(0x80)

/* Interrupt mask registers */
#define AB3550_IMR1 0x29
#define AB3550_IMR2 0x2a
#define AB3550_IMR3 0x2b
#define AB3550_IMR4 0x2c
#define AB3550_IMR5 0x2d

enum ab3550_devid {
	AB3550_DEVID_ADC,
	AB3550_DEVID_DAC,
	AB3550_DEVID_LEDS,
	AB3550_DEVID_POWER,
	AB3550_DEVID_REGULATORS,
	AB3550_DEVID_SIM,
	AB3550_DEVID_UART,
	AB3550_DEVID_RTC,
	AB3550_DEVID_CHARGER,
	AB3550_DEVID_FUELGAUGE,
	AB3550_DEVID_VIBRATOR,
	AB3550_DEVID_CODEC,
	AB3550_NUM_DEVICES,
};

/**
 * struct abx500_init_setting
 * Initial value of the registers for driver to use during setup.
 */
struct abx500_init_settings {
	u8 bank;
	u8 reg;
	u8 setting;
};

/**
 * struct ab3550_platform_data
 * Data supplied to initialize board connections to the AB3550
 */
struct ab3550_platform_data {
	struct {unsigned int base; unsigned int count; } irq;
	void *dev_data[AB3550_NUM_DEVICES];
	size_t dev_data_sz[AB3550_NUM_DEVICES];
	struct abx500_init_settings *init_settings;
	unsigned int init_settings_sz;
};

/**
 *
 * ab5500
 *
 */

enum ab5500_devid {
	AB5500_DEVID_ADC,
	AB5500_DEVID_LEDS,
	AB5500_DEVID_POWER,
	AB5500_DEVID_REGULATORS,
	AB5500_DEVID_SIM,
	AB5500_DEVID_RTC,
	AB5500_DEVID_CHARGER,
	AB5500_DEVID_FUELGAUGE,
	AB5500_DEVID_VIBRATOR,
	AB5500_DEVID_CODEC,
	AB5500_DEVID_USB,
	AB5500_DEVID_OTP,
	AB5500_DEVID_VIDEO,
	AB5500_DEVID_DBIECI,
	AB5500_DEVID_ONSWA,
	AB5500_NUM_DEVICES,
};

enum ab5500_banks {
	AB5500_BANK_VIT_IO_I2C_CLK_TST_OTP = 0,
	AB5500_BANK_VDDDIG_IO_I2C_CLK_TST = 1,
	AB5500_BANK_VDENC = 2,
	AB5500_BANK_SIM_USBSIM  = 3,
	AB5500_BANK_LED = 4,
	AB5500_BANK_ADC  = 5,
	AB5500_BANK_RTC  = 6,
	AB5500_BANK_STARTUP  = 7,
	AB5500_BANK_DBI_ECI  = 8,
	AB5500_BANK_CHG  = 9,
	AB5500_BANK_FG_BATTCOM_ACC = 10,
	AB5500_BANK_USB = 11,
	AB5500_BANK_IT = 12,
	AB5500_BANK_VIBRA = 13,
	AB5500_BANK_AUDIO_HEADSETUSB = 14,
	AB5500_NUM_BANKS = 15,
};

enum ab5500_banks_addr {
	AB5500_ADDR_VIT_IO_I2C_CLK_TST_OTP = 0x4A,
	AB5500_ADDR_VDDDIG_IO_I2C_CLK_TST = 0x4B,
	AB5500_ADDR_VDENC = 0x06,
	AB5500_ADDR_SIM_USBSIM  = 0x04,
	AB5500_ADDR_LED = 0x10,
	AB5500_ADDR_ADC  = 0x0A,
	AB5500_ADDR_RTC  = 0x0F,
	AB5500_ADDR_STARTUP  = 0x03,
	AB5500_ADDR_DBI_ECI  = 0x07,
	AB5500_ADDR_CHG  = 0x0B,
	AB5500_ADDR_FG_BATTCOM_ACC = 0x0C,
	AB5500_ADDR_USB = 0x05,
	AB5500_ADDR_IT = 0x0E,
	AB5500_ADDR_VIBRA = 0x02,
	AB5500_ADDR_AUDIO_HEADSETUSB = 0x0D,
};

/*
 * Interrupt register offsets
 * Bank : 0x0E
 */
#define AB5500_IT_SOURCE0_REG		0x20
#define AB5500_IT_SOURCE1_REG		0x21
#define AB5500_IT_SOURCE2_REG		0x22
#define AB5500_IT_SOURCE3_REG		0x23
#define AB5500_IT_SOURCE4_REG		0x24
#define AB5500_IT_SOURCE5_REG		0x25
#define AB5500_IT_SOURCE6_REG		0x26
#define AB5500_IT_SOURCE7_REG		0x27
#define AB5500_IT_SOURCE8_REG		0x28
#define AB5500_IT_SOURCE9_REG		0x29
#define AB5500_IT_SOURCE10_REG		0x2A
#define AB5500_IT_SOURCE11_REG		0x2B
#define AB5500_IT_SOURCE12_REG		0x2C
#define AB5500_IT_SOURCE13_REG		0x2D
#define AB5500_IT_SOURCE14_REG		0x2E
#define AB5500_IT_SOURCE15_REG		0x2F
#define AB5500_IT_SOURCE16_REG		0x30
#define AB5500_IT_SOURCE17_REG		0x31
#define AB5500_IT_SOURCE18_REG		0x32
#define AB5500_IT_SOURCE19_REG		0x33
#define AB5500_IT_SOURCE20_REG		0x34
#define AB5500_IT_SOURCE21_REG		0x35
#define AB5500_IT_SOURCE22_REG		0x36
#define AB5500_IT_SOURCE23_REG		0x37

#define AB5500_NUM_IRQ_REGS		23

/**
 * struct ab5500
 * @access_mutex: lock out concurrent accesses to the AB registers
 * @dev: a pointer to the device struct for this chip driver
 * @ab5500_irq: the analog baseband irq
 * @irq_base: the platform configuration irq base for subdevices
 * @chip_name: name of this chip variant
 * @chip_id: 8 bit chip ID for this chip variant
 * @irq_lock: a lock to protect the mask
 * @abb_events: a local bit mask of the prcmu wakeup events
 * @event_mask: a local copy of the mask event registers
 * @last_event_mask: a copy of the last event_mask written to hardware
 * @startup_events: a copy of the first reading of the event registers
 * @startup_events_read: whether the first events have been read
 */
struct ab5500 {
	struct mutex access_mutex;
	struct device *dev;
	unsigned int ab5500_irq;
	unsigned int irq_base;
	char chip_name[32];
	u8 chip_id;
	struct mutex irq_lock;
	u32 abb_events;
	u8 mask[AB5500_NUM_IRQ_REGS];
	u8 oldmask[AB5500_NUM_IRQ_REGS];
	u8 startup_events[AB5500_NUM_IRQ_REGS];
	bool startup_events_read;
#ifdef CONFIG_DEBUG_FS
	unsigned int debug_bank;
	unsigned int debug_address;
#endif
};

struct ab5500_regulator_platform_data;
struct ab5500_platform_data {
	struct {unsigned int base; unsigned int count; } irq;
	void *dev_data[AB5500_NUM_DEVICES];
	size_t dev_data_sz[AB5500_NUM_DEVICES];
	struct abx500_init_settings *init_settings;
	unsigned int init_settings_sz;
	bool pm_power_off;
	struct ab5500_regulator_platform_data *regulator;
};


int abx500_set_register_interruptible(struct device *dev, u8 bank, u8 reg,
	u8 value);
int abx500_get_register_interruptible(struct device *dev, u8 bank, u8 reg,
	u8 *value);
int abx500_get_register_page_interruptible(struct device *dev, u8 bank,
	u8 first_reg, u8 *regvals, u8 numregs);
int abx500_set_register_page_interruptible(struct device *dev, u8 bank,
	u8 first_reg, u8 *regvals, u8 numregs);
/**
 * abx500_mask_and_set_register_inerruptible() - Modifies selected bits of a
 *	target register
 *
 * @dev: The AB sub device.
 * @bank: The i2c bank number.
 * @bitmask: The bit mask to use.
 * @bitvalues: The new bit values.
 *
 * Updates the value of an AB register:
 * value -> ((value & ~bitmask) | (bitvalues & bitmask))
 */
int abx500_mask_and_set_register_interruptible(struct device *dev, u8 bank,
	u8 reg, u8 bitmask, u8 bitvalues);
int abx500_get_chip_id(struct device *dev);
int abx500_event_registers_startup_state_get(struct device *dev, u8 *event);
int abx500_startup_irq_enabled(struct device *dev, unsigned int irq);

#define abx500_get	abx500_get_register_interruptible
#define abx500_set	abx500_set_register_interruptible
#define abx500_get_page	abx500_get_register_page_interruptible
#define abx500_set_page	abx500_set_register_page_interruptible
#define abx500_mask_and_set	abx500_mask_and_set_register_interruptible

struct abx500_ops {
	int (*get_chip_id) (struct device *);
	int (*get_register) (struct device *, u8, u8, u8 *);
	int (*set_register) (struct device *, u8, u8, u8);
	int (*get_register_page) (struct device *, u8, u8, u8 *, u8);
	int (*set_register_page) (struct device *, u8, u8, u8 *, u8);
	int (*mask_and_set_register) (struct device *, u8, u8, u8, u8);
	int (*event_registers_startup_state_get) (struct device *, u8 *);
	int (*startup_irq_enabled) (struct device *, unsigned int);
};

int abx500_register_ops(struct device *dev, struct abx500_ops *ops);
void abx500_remove_ops(struct device *dev);
#endif
