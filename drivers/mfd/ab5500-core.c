/*
 * Copyright (C) 2007-2010 ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 * Low-level core for exclusive access to the AB5500 IC on the I2C bus
 * and some basic chip-configuration.
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com>
 * Author: Mattias Nilsson <mattias.i.nilsson@stericsson.com>
 * Author: Mattias Wallin <mattias.wallin@stericsson.com>
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>
 * Author: Karl Komierowski  <karl.komierowski@stericsson.com>
 */

#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/mfd/abx500.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/mfd/core.h>
#include <linux/version.h>
#include <mach/prcmu-db5500.h>

#define AB5500_NAME_STRING "ab5500"
#define AB5500_ID_FORMAT_STRING "AB5500 %s"
#define AB5500_NUM_EVENT_REG 23

/* These are the only registers inside AB5500 used in this main file */

/* Read/write operation values. */
#define AB5500_PERM_RD (0x01)
#define AB5500_PERM_WR (0x02)

/* Read/write permissions. */
#define AB5500_PERM_RO (AB5500_PERM_RD)
#define AB5500_PERM_RW (AB5500_PERM_RD | AB5500_PERM_WR)

#define AB5500_MASK_BASE (0x60)
#define AB5500_MASK_END (0x79)
#define AB5500_CHIP_ID (0x20)

/**
 * struct ab5500
 * @access_mutex: lock out concurrent accesses to the AB registers
 * @dev: a pointer to the device struct for this chip driver
 * @ab5500_irq: the analog baseband irq
 * @irq_base: the platform configuration irq base for subdevices
 * @chip_name: name of this chip variant
 * @chip_id: 8 bit chip ID for this chip variant
 * @mask_work: a worker for writing to mask registers
 * @event_lock: a lock to protect the event_mask
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
	struct work_struct mask_work;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	struct work_struct irq_work;
#endif
	spinlock_t event_lock;
	u32 abb_events;
	u8 event_mask[AB5500_NUM_EVENT_REG];
	u8 last_event_mask[AB5500_NUM_EVENT_REG];
	u8 startup_events[AB5500_NUM_EVENT_REG];
	bool startup_events_read;
#ifdef CONFIG_DEBUG_FS
	unsigned int debug_bank;
	unsigned int debug_address;
#endif
};

/**
 * struct ab5500_bank
 * @slave_addr: I2C slave_addr found in AB5500 specification
 * @name: Documentation name of the bank. For reference
 */
struct ab5500_bank {
	u8 slave_addr;
	const char *name;
};

static const struct ab5500_bank bankinfo[AB5500_NUM_BANKS] = {
	[AB5500_BANK_VIT_IO_I2C_CLK_TST_OTP] = {0x4A, "VIT_IO_I2C_CLK_TST_OTP"},
	[AB5500_BANK_VDDDIG_IO_I2C_CLK_TST] = {0x4B, "VDDDIG_IO_I2C_CLK_TST"},
	[AB5500_BANK_VDENC] = {0x06, "VDENC"},
	[AB5500_BANK_SIM_USBSIM] = {0x04, "SIM_USBSIM"},
	[AB5500_BANK_LED] = {0x10, "LED"},
	[AB5500_BANK_ADC] = {0x0A, "ADC"},
	[AB5500_BANK_RTC] = {0x0F, "RTC"},
	[AB5500_BANK_STARTUP] = {0x03, "STARTUP"},
	[AB5500_BANK_DBI_ECI] = {0x07, "DBI-ECI"},
	[AB5500_BANK_CHG] = {0x0B, "CHG"},
	[AB5500_BANK_FG_BATTCOM_ACC] = {0x0C, "FG_BATCOM_ACC"},
	[AB5500_BANK_USB] = {0x05, "USB"},
	[AB5500_BANK_IT] = {0x0E, "IT"},
	[AB5500_BANK_VIBRA] = {0x02, "VIBRA"},
	[AB5500_BANK_AUDIO_HEADSETUSB] = {0x0D, "AUDIO_HEADSETUSB"},
};

/**
 * struct ab5500_reg_range
 * @first: the first address of the range
 * @last: the last address of the range
 * @perm: access permissions for the range
 */
struct ab5500_reg_range {
	u8 first;
	u8 last;
	u8 perm;
};

/**
 * struct ab5500_i2c_ranges
 * @count: the number of ranges in the list
 * @range: the list of register ranges
 */
struct ab5500_i2c_ranges {
	u8 nranges;
	u8 bankid;
	const struct ab5500_reg_range *range;
};

/**
 * struct ab5500_i2c_banks
 * @count: the number of ranges in the list
 * @range: the list of register ranges
 */
struct ab5500_i2c_banks {
	u8 nbanks;
	const struct ab5500_i2c_ranges *bank;
};

/*
 * Permissible register ranges for reading and writing per device and bank.
 *
 * The ranges must be listed in increasing address order, and no overlaps are
 * allowed. It is assumed that write permission implies read permission
 * (i.e. only RO and RW permissions should be used).  Ranges with write
 * permission must not be split up.
 */

#define NO_RANGE {.count = 0, .range = NULL,}

static struct ab5500_i2c_banks ab5500_bank_ranges[AB5500_NUM_DEVICES] = {
	[AB5500_DEVID_ADC] =  {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_ADC,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x20,
						.last = 0x58,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_LEDS] =  {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_LED,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x00,
						.last = 0x0C,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	/* What registers should this device access?*/
	[AB5500_DEVID_POWER] =   {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_CHG,
				.nranges = 2,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x00,
						.last = 0x03,
						.perm = AB5500_PERM_RW,
					},
					{
						.first = 0x10,
						.last = 0x30,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_REGULATORS] =   {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_STARTUP,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x50,
						.last = 0xE0,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_SIM] =   {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_SIM_USBSIM,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x13,
						.last = 0x19,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_RTC] =   {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_LED,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x00,
						.last = 0x0C,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_CHARGER] =   {
		.nbanks = 2,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_CHG,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x10,
						.last = 0x18,
						.perm = AB5500_PERM_RW,
					},
				},
			},
			{
				.bankid = AB5500_BANK_ADC,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x1F,
						.last = 0x58,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_FUELGAUGE] =   {
		.nbanks = 2,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_FG_BATTCOM_ACC,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x00,
						.last = 0x10,
						.perm = AB5500_PERM_RW,
					},
				},
			},
			{
				.bankid = AB5500_BANK_ADC,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x20,
						.last = 0x58,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_VIBRATOR] =   {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_VIBRA,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x10,
						.last = 0x13,
						.perm = AB5500_PERM_RW,
					},
				},
			},
		},
	},
	[AB5500_DEVID_CODEC] =   {
		.nbanks = 1,
		.bank = (struct ab5500_i2c_ranges[]) {
			{
				.bankid = AB5500_BANK_AUDIO_HEADSETUSB,
				.nranges = 1,
				.range = (struct ab5500_reg_range[]) {
					{
						.first = 0x00,
						.last = 0x48,
						.perm = AB5500_PERM_RW,
					},

				},
			},
		},
	},
};

/* I appologize for the resource names beeing a mix of upper case
 * and lower case but I want them to be exact as the documentation */
static struct mfd_cell ab5500_devs[AB5500_NUM_DEVICES] = {
	[AB5500_DEVID_LEDS] = {
		.name = "ab5500-leds",
		.id = AB5500_DEVID_LEDS,
	},
	[AB5500_DEVID_POWER] = {
		.name = "ab5500-power",
		.id = AB5500_DEVID_POWER,
	},
	[AB5500_DEVID_REGULATORS] = {
		.name = "ab5500-regulators",
		.id = AB5500_DEVID_REGULATORS,
	},
	[AB5500_DEVID_SIM] = {
		.name = "ab5500-sim",
		.id = AB5500_DEVID_SIM,
		.num_resources = 1,
		.resources = (struct resource[]) {
			{
				.name = "SIMOFF",
				.flags = IORESOURCE_IRQ,
				.start = 16, /*rising*/
				.end = 17, /*falling*/
			},
		},
	},
	[AB5500_DEVID_RTC] = {
		.name = "ab5500-rtc",
		.id = AB5500_DEVID_RTC,
	},
	[AB5500_DEVID_CHARGER] = {
		.name = "ab5500-charger",
		.id = AB5500_DEVID_CHARGER,
	},
	[AB5500_DEVID_ADC] = {
		.name = "ab5500-adc",
		.id = AB5500_DEVID_ADC,
		.num_resources = 10,
		.resources = (struct resource[]) {
			{
				.name = "TRIGGER-0",
				.flags = IORESOURCE_IRQ,
				.start = 0,
				.end = 0,
			},
			{
				.name = "TRIGGER-1",
				.flags = IORESOURCE_IRQ,
				.start = 1,
				.end = 1,
			},
			{
				.name = "TRIGGER-2",
				.flags = IORESOURCE_IRQ,
				.start = 2,
				.end = 2,
			},
			{
				.name = "TRIGGER-3",
				.flags = IORESOURCE_IRQ,
				.start = 3,
				.end = 3,
			},
			{
				.name = "TRIGGER-4",
				.flags = IORESOURCE_IRQ,
				.start = 4,
				.end = 4,
			},
			{
				.name = "TRIGGER-5",
				.flags = IORESOURCE_IRQ,
				.start = 5,
				.end = 5,
			},
			{
				.name = "TRIGGER-6",
				.flags = IORESOURCE_IRQ,
				.start = 6,
				.end = 6,
			},
			{
				.name = "TRIGGER-7",
				.flags = IORESOURCE_IRQ,
				.start = 7,
				.end = 7,
			},
			{
				.name = "TRIGGER-VBAT-TXON",
				.flags = IORESOURCE_IRQ,
				.start = 9,
				.end = 9,
			},
			{
				.name = "TRIGGER-VBAT",
				.flags = IORESOURCE_IRQ,
				.start = 8,
				.end = 8,
			},
		},
	},
	[AB5500_DEVID_FUELGAUGE] = {
		.name = "ab5500-fuelgauge",
		.id = AB5500_DEVID_FUELGAUGE,
		.num_resources = 6,
		.resources = (struct resource[]) {
			{
				.name = "Batt_attach",
				.flags = IORESOURCE_IRQ,
				.start = 61,
				.end = 61,
			},
			{
				.name = "Batt_removal",
				.flags = IORESOURCE_IRQ,
				.start = 62,
				.end = 62,
			},
			{
				.name = "UART_framing",
				.flags = IORESOURCE_IRQ,
				.start = 63,
				.end = 63,
			},
			{
				.name = "UART_overrun",
				.flags = IORESOURCE_IRQ,
				.start = 64,
				.end = 64,
			},
			{
				.name = "UART_Rdy_RX",
				.flags = IORESOURCE_IRQ,
				.start = 65,
				.end = 65,
			},
			{
				.name = "UART_Rdy_TX",
				.flags = IORESOURCE_IRQ,
				.start = 66,
				.end = 66,
			},
		},
	},
	[AB5500_DEVID_VIBRATOR] = {
		.name = "ab5500-vibrator",
		.id = AB5500_DEVID_VIBRATOR,
	},
	[AB5500_DEVID_CODEC] = {
		.name = "ab5500-codec",
		.id = AB5500_DEVID_CODEC,
		.num_resources = 3,
		.resources = (struct resource[]) {
			{
				.name = "audio_spkr1_ovc",
				.flags = IORESOURCE_IRQ,
				.start = 77,
				.end = 77,
			},
			{
				.name = "audio_plllocked",
				.flags = IORESOURCE_IRQ,
				.start = 78,
				.end = 78,
			},
			{
				.name = "audio_spkr2_ovc",
				.flags = IORESOURCE_IRQ,
				.start = 140,
				.end = 140,
			},
		},
	},
	[AB5500_DEVID_USB] = {
		.name = "ab5500-usb",
		.id = AB5500_DEVID_USB,
		.num_resources = 35,
		.resources = (struct resource[]) {
			{
				.name = "DCIO",
				.flags = IORESOURCE_IRQ,
				.start = 67,
				.end = 68,
			},
			{
				.name = "VBUS",
				.flags = IORESOURCE_IRQ,
				.start = 69,
				.end = 70,
			},
			{
				.name = "CHGstate_10_PCVBUSchg",
				.flags = IORESOURCE_IRQ,
				.start = 71,
				.end = 71,
			},
			{
				.name = "DCIOreverse_ovc",
				.flags = IORESOURCE_IRQ,
				.start = 72,
				.end = 72,
			},
			{
				.name = "USBCharDetDone",
				.flags = IORESOURCE_IRQ,
				.start = 73,
				.end = 73,
			},
			{
				.name = "DCIO_no_limit",
				.flags = IORESOURCE_IRQ,
				.start = 74,
				.end = 74,
			},
			{
				.name = "USB_suspend",
				.flags = IORESOURCE_IRQ,
				.start = 75,
				.end = 75,
			},
			{
				.name = "DCIOreverse_fwdcurrent",
				.flags = IORESOURCE_IRQ,
				.start = 76,
				.end = 76,
			},
			{
				.name = "Vbus_Imeasmax_change",
				.flags = IORESOURCE_IRQ,
				.start = 79,
				.end = 80,
			},
			{
				.name = "OVV",
				.flags = IORESOURCE_IRQ,
				.start = 117,
				.end = 117,
			},
			{
				.name = "USBcharging_NOTok",
				.flags = IORESOURCE_IRQ,
				.start = 123,
				.end = 123,
			},
			{
				.name = "usb_adp_sensoroff",
				.flags = IORESOURCE_IRQ,
				.start = 126,
				.end = 126,
			},
			{
				.name = "usb_adp_probeplug",
				.flags = IORESOURCE_IRQ,
				.start = 127,
				.end = 127,
			},
			{
				.name = "usb_adp_sinkerror",
				.flags = IORESOURCE_IRQ,
				.start = 128,
				.end = 128,
			},
			{
				.name = "usb_adp_sourceerror",
				.flags = IORESOURCE_IRQ,
				.start = 129,
				.end = 129,
			},
			{
				.name = "usb_idgnd",
				.flags = IORESOURCE_IRQ,
				.start = 130,
				.end = 131,
			},
			{
				.name = "usb_iddetR1",
				.flags = IORESOURCE_IRQ,
				.start = 132,
				.end = 133,
			},
			{
				.name = "usb_iddetR2",
				.flags = IORESOURCE_IRQ,
				.start = 134,
				.end = 135,
			},
			{
				.name = "usb_iddetR3",
				.flags = IORESOURCE_IRQ,
				.start = 136,
				.end = 137,
			},
			{
				.name = "usb_iddetR4",
				.flags = IORESOURCE_IRQ,
				.start = 138,
				.end = 139,
			},
			{
				.name = "CharTempWindowOk",
				.flags = IORESOURCE_IRQ,
				.start = 143,
				.end = 144,
			},
			{
				.name = "USB_SprDetect",
				.flags = IORESOURCE_IRQ,
				.start = 145,
				.end = 145,
			},
			{
				.name = "usb_adp_probe_unplug",
				.flags = IORESOURCE_IRQ,
				.start = 146,
				.end = 146,
			},
			{
				.name = "VBUSChDrop",
				.flags = IORESOURCE_IRQ,
				.start = 147,
				.end = 148,
			},
			{
				.name = "dcio_char_rec_done",
				.flags = IORESOURCE_IRQ,
				.start = 149,
				.end = 149,
			},
			{
				.name = "Charging_stopped_by_temp",
				.flags = IORESOURCE_IRQ,
				.start = 150,
				.end = 150,
			},
			{
				.name = "CHGstate_11_SafeModeVBUS",
				.flags = IORESOURCE_IRQ,
				.start = 169,
				.end = 169,
			},
			{
				.name = "CHGstate_12_comletedVBUS",
				.flags = IORESOURCE_IRQ,
				.start = 170,
				.end = 170,
			},
			{
				.name = "CHGstate_13_completedVBUS",
				.flags = IORESOURCE_IRQ,
				.start = 171,
				.end = 171,
			},
			{
				.name = "CHGstate_14_FullChgDCIO",
				.flags = IORESOURCE_IRQ,
				.start = 172,
				.end = 172,
			},
			{
				.name = "CHGstate_15_SafeModeDCIO",
				.flags = IORESOURCE_IRQ,
				.start = 173,
				.end = 173,
			},
			{
				.name = "CHGstate_16_OFFsuspendDCIO",
				.flags = IORESOURCE_IRQ,
				.start = 174,
				.end = 174,
			},
			{
				.name = "CHGstate_17_completedDCIO",
				.flags = IORESOURCE_IRQ,
				.start = 175,
				.end = 175,
			},
			{
				.name = "o_it_dcio_char_rec_notok",
				.flags = IORESOURCE_IRQ,
				.start = 176,
				.end = 176,
			},
			{
				.name = "usb_link_update",
				.flags = IORESOURCE_IRQ,
				.start = 177,
				.end = 177,
			},
		},
	},
	[AB5500_DEVID_OTP] = {
		.name = "ab5500-otp",
		.id = AB5500_DEVID_OTP,
	},
	[AB5500_DEVID_VIDEO] = {
		.name = "ab5500-video",
		.id = AB5500_DEVID_VIDEO,
		.num_resources = 1,
		.resources = (struct resource[]) {
			{
				.name = "plugTVdet",
				.flags = IORESOURCE_IRQ,
				.start = 111,
				.end = 111,
			},
		},
	},
	[AB5500_DEVID_DBIECI] = {
		.name = "ab5500-dbieci",
		.id = AB5500_DEVID_DBIECI,
		.num_resources = 10,
		.resources = (struct resource[]) {
			{
				.name = "COLL",
				.flags = IORESOURCE_IRQ,
				.start = 112,
				.end = 112,
			},
			{
				.name = "RESERR",
				.flags = IORESOURCE_IRQ,
				.start = 113,
				.end = 113,
			},
			{
				.name = "FRAERR",
				.flags = IORESOURCE_IRQ,
				.start = 114,
				.end = 114,
			},
			{
				.name = "COMERR",
				.flags = IORESOURCE_IRQ,
				.start = 115,
				.end = 115,
			},
			{
				.name = "BSI_indicator",
				.flags = IORESOURCE_IRQ,
				.start = 116,
				.end = 116,
			},
			{
				.name = "SPDSET",
				.flags = IORESOURCE_IRQ,
				.start = 118,
				.end = 118,
			},
			{
				.name = "DSENT",
				.flags = IORESOURCE_IRQ,
				.start = 119,
				.end = 119,
			},
			{
				.name = "DREC",
				.flags = IORESOURCE_IRQ,
				.start = 120,
				.end = 120,
			},
			{
				.name = "ACCINT",
				.flags = IORESOURCE_IRQ,
				.start = 121,
				.end = 121,
			},
			{
				.name = "NOPINT",
				.flags = IORESOURCE_IRQ,
				.start = 122,
				.end = 122,
			},
		},
	},
};

/*
 * This stubbed prcmu functionality should be removed when the prcmu driver
 * implements it.
 */
static u8 prcmu_event_buf[AB5500_NUM_EVENT_REG];

void prcmu_get_abb_event_buf(u8 **buf)
{
	*buf = prcmu_event_buf;
}

/*
 * Functionality for getting/setting register values.
 */
static int get_register_interruptible(struct ab5500 *ab, u8 bank, u8 reg,
	u8 *value)
{
	int err;

	if (bank >= AB5500_NUM_BANKS)
		return -EINVAL;

	err = mutex_lock_interruptible(&ab->access_mutex);
	if (err)
		return err;
	err = db5500_prcmu_abb_read(bankinfo[bank].slave_addr, reg, value, 1);

	mutex_unlock(&ab->access_mutex);
	return err;
}

static int get_register_page_interruptible(struct ab5500 *ab, u8 bank,
	u8 first_reg, u8 *regvals, u8 numregs)
{
	int err;

	if (bank >= AB5500_NUM_BANKS)
		return -EINVAL;

	/* The hardware limit for get page is 4 */
	if (numregs > 4)
		return -EINVAL;

	err = mutex_lock_interruptible(&ab->access_mutex);
	if (err)
		return err;

	err = db5500_prcmu_abb_read(bankinfo[bank].slave_addr, first_reg,
		regvals, numregs);

	mutex_unlock(&ab->access_mutex);
	return err;
}

static int mask_and_set_register_interruptible(struct ab5500 *ab, u8 bank,
	u8 reg, u8 bitmask, u8 bitvalues)
{
	int err = 0;

	if (bank >= AB5500_NUM_BANKS)
		return -EINVAL;

	if (bitmask) {
		u8 buf;

		err = mutex_lock_interruptible(&ab->access_mutex);
		if (err)
			return err;

		if (bitmask == 0xFF) /* No need to read in this case. */
			buf = bitvalues;
		else { /* Read and modify the register value. */
			err = db5500_prcmu_abb_read(bankinfo[bank].slave_addr,
				reg, &buf, 1);
			if (err)
				return err;

			buf = ((~bitmask & buf) | (bitmask & bitvalues));
		}
		/* Write the new value. */
		err = db5500_prcmu_abb_write(bankinfo[bank].slave_addr, reg,
					     &buf, 1);

		mutex_unlock(&ab->access_mutex);
	}
	return err;
}

/*
 * Read/write permission checking functions.
 */
static const struct ab5500_i2c_ranges *get_bankref(u8 devid, u8 bank)
{
	u8 i;

	if (devid < AB5500_NUM_DEVICES) {
		for (i = 0; i < ab5500_bank_ranges[devid].nbanks; i++) {
			if (ab5500_bank_ranges[devid].bank[i].bankid == bank)
				return &ab5500_bank_ranges[devid].bank[i];
		}
	}
	return NULL;
}

static bool page_write_allowed(u8 devid, u8 bank, u8 first_reg, u8 last_reg)
{
	u8 i; /* range loop index */
	const struct ab5500_i2c_ranges *bankref;

	bankref = get_bankref(devid, bank);
	if (bankref == NULL || last_reg < first_reg)
		return false;

	for (i = 0; i < bankref->nranges; i++) {
		if (first_reg < bankref->range[i].first)
			break;
		if ((last_reg <= bankref->range[i].last) &&
			(bankref->range[i].perm & AB5500_PERM_WR))
			return true;
	}
	return false;
}

static bool reg_write_allowed(u8 devid, u8 bank, u8 reg)
{
	return page_write_allowed(devid, bank, reg, reg);
}

static bool page_read_allowed(u8 devid, u8 bank, u8 first_reg, u8 last_reg)
{
	u8 i;
	const struct ab5500_i2c_ranges *bankref;

	bankref = get_bankref(devid, bank);
	if (bankref == NULL || last_reg < first_reg)
		return false;


	/* Find the range (if it exists in the list) that includes first_reg. */
	for (i = 0; i < bankref->nranges; i++) {
		if (first_reg < bankref->range[i].first)
			return false;
		if (first_reg <= bankref->range[i].last)
			break;
	}
	/* Make sure that the entire range up to and including last_reg is
	 * readable. This may span several of the ranges in the list.
	 */
	while ((i < bankref->nranges) &&
		(bankref->range[i].perm & AB5500_PERM_RD)) {
		if (last_reg <= bankref->range[i].last)
			return true;
		if ((++i >= bankref->nranges) ||
			(bankref->range[i].first !=
				(bankref->range[i - 1].last + 1))) {
			break;
		}
	}
	return false;
}

static bool reg_read_allowed(u8 devid, u8 bank, u8 reg)
{
	return page_read_allowed(devid, bank, reg, reg);
}


/*
 * The exported register access functionality.
 */
int ab5500_get_chip_id(struct device *dev)
{
	struct ab5500 *ab = dev_get_drvdata(dev->parent);

	return (int)ab->chip_id;
}

int ab5500_mask_and_set_register_interruptible(struct device *dev, u8 bank,
	u8 reg, u8 bitmask, u8 bitvalues)
{
	struct ab5500 *ab;
	struct platform_device *pdev = to_platform_device(dev);

	if ((AB5500_NUM_BANKS <= bank) ||
		!reg_write_allowed(pdev->id, bank, reg))
		return -EINVAL;

	ab = dev_get_drvdata(dev->parent);
	return mask_and_set_register_interruptible(ab, bank, reg,
		bitmask, bitvalues);
}

int ab5500_set_register_interruptible(struct device *dev, u8 bank, u8 reg,
	u8 value)
{
	return ab5500_mask_and_set_register_interruptible(dev, bank, reg, 0xFF,
		value);
}

int ab5500_get_register_interruptible(struct device *dev, u8 bank, u8 reg,
	u8 *value)
{
	struct ab5500 *ab;
	struct platform_device *pdev = to_platform_device(dev);

	if ((AB5500_NUM_BANKS <= bank) ||
		!reg_read_allowed(pdev->id, bank, reg))
		return -EINVAL;

	ab = dev_get_drvdata(dev->parent);
	return get_register_interruptible(ab, bank, reg, value);
}

int ab5500_get_register_page_interruptible(struct device *dev, u8 bank,
	u8 first_reg, u8 *regvals, u8 numregs)
{
	struct ab5500 *ab;
	struct platform_device *pdev = to_platform_device(dev);

	if ((AB5500_NUM_BANKS <= bank) ||
		!page_read_allowed(pdev->id, bank,
			first_reg, (first_reg + numregs - 1)))
		return -EINVAL;

	ab = dev_get_drvdata(dev->parent);
	return get_register_page_interruptible(ab, bank, first_reg, regvals,
		numregs);
}

int ab5500_event_registers_startup_state_get(struct device *dev, u8 *event)
{
	struct ab5500 *ab;

	ab = dev_get_drvdata(dev->parent);
	if (!ab->startup_events_read)
		return -EAGAIN; /* Try again later */

	memcpy(event, ab->startup_events, AB5500_NUM_EVENT_REG);
	return 0;
}

int ab5500_startup_irq_enabled(struct device *dev, unsigned int irq)
{
	struct ab5500 *ab;
	bool val;

	ab = get_irq_chip_data(irq);
	irq -= ab->irq_base;
	val = ((ab->startup_events[irq / 8] & BIT(irq % 8)) != 0);

	return val;
}

static struct abx500_ops ab5500_ops = {
	.get_chip_id = ab5500_get_chip_id,
	.get_register = ab5500_get_register_interruptible,
	.set_register = ab5500_set_register_interruptible,
	.get_register_page = ab5500_get_register_page_interruptible,
	.set_register_page = NULL,
	.mask_and_set_register = ab5500_mask_and_set_register_interruptible,
	.event_registers_startup_state_get =
		ab5500_event_registers_startup_state_get,
	.startup_irq_enabled = ab5500_startup_irq_enabled,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
static irqreturn_t ab5500_irq_handler(int irq, void *data)
{
	struct ab5500 *ab = data;

	/*
	 * Disable the IRQ and dispatch a worker to handle the
	 * event. Since the chip resides on I2C this is slow
	 * stuff and we will re-enable the interrupts once the
	 * worker has finished.
	 */
	disable_irq_nosync(irq);
	schedule_work(&ab->irq_work);
	return IRQ_HANDLED;
}

static void ab5500_irq_work(struct work_struct *work)
{
	struct ab5500 *ab = container_of(work, struct ab5500, irq_work);
	u8 i;
	u8 *e = 0;
	u8 events[AB5500_NUM_EVENT_REG];
	unsigned long flags;

	prcmu_get_abb_event_buf(&e);

	spin_lock_irqsave(&ab->event_lock, flags);
	for (i = 0; i < AB5500_NUM_EVENT_REG; i++)
		events[i] = e[i] & ~ab->event_mask[i];
	spin_unlock_irqrestore(&ab->event_lock, flags);

	local_irq_disable();
	for (i = 0; i < AB5500_NUM_EVENT_REG; i++) {
		u8 bit;
		u8 event_reg;

		dev_dbg(ab->dev, "IRQ Event[%d]: 0x%2x\n",
			i, events[i]);

		event_reg = events[i];
		for (bit = 0; event_reg; bit++, event_reg /= 2) {
			if (event_reg % 2) {
				unsigned int irq;
				struct irq_desc *desc;

				irq = ab->irq_base + (i * 8) + bit;
				desc = irq_to_desc(irq);
				if (desc->status & IRQ_DISABLED)
					note_interrupt(irq, desc, IRQ_NONE);
				else
					desc->handle_irq(irq, desc);
			}
		}
	}
	local_irq_enable();
	/* By now the IRQ should be acked and deasserted so enable it again */
	enable_irq(ab->ab5500_irq);
}

#else

static irqreturn_t ab5500_irq_handler(int irq, void *data)
{
	struct ab5500 *ab = data;
	u8 i;
	u8 *e = 0;
	u8 events[AB5500_NUM_EVENT_REG];

	prcmu_get_abb_event_buf(&e);

	spin_lock(&ab->event_lock);
	for (i = 0; i < AB5500_NUM_EVENT_REG; i++)
		events[i] = e[i] & ~ab->event_mask[i];
	spin_unlock(&ab->event_lock);

	for (i = 0; i < AB5500_NUM_EVENT_REG; i++) {
		u8 bit;
		u8 event_reg;

		dev_dbg(ab->dev, "IRQ Event[%d]: 0x%2x\n",
			i, events[i]);

		event_reg = events[i];
		for (bit = 0; event_reg; bit++, event_reg /= 2) {
			if (event_reg % 2) {
				unsigned int irq;

				irq = ab->irq_base + (i * 8) + bit;
				generic_handle_irq(irq);
			}
		}
	}

	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_DEBUG_FS
static struct ab5500_i2c_ranges debug_ranges[AB5500_NUM_BANKS] = {
	[AB5500_BANK_LED] = {
		.nranges = 1,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x0C,
			},
		},
	},
	[AB5500_BANK_ADC] = {
		.nranges = 4,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x1F,
				.last = 0x24,
			},
			{
				.first = 0x26,
				.last = 0x2D,
			},
			{
				.first = 0x2F,
				.last = 0x35,
			},
			{
				.first = 0x37,
				.last = 0x58,
			},
		},
	},
	[AB5500_BANK_RTC] = {
		.nranges = 2,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x04,
			},
			{
				.first = 0x06,
				.last = 0x0C,
			},
		},
	},
	[AB5500_BANK_STARTUP] = {
		.nranges = 12,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x01,
			},
			{
				.first = 0x1F,
				.last = 0x1F,
			},
			{
				.first = 0x2E,
				.last = 0x30,
			},
			{
				.first = 0x50,
				.last = 0x51,
			},
			{
				.first = 0x60,
				.last = 0x61,
			},
			{
				.first = 0x66,
				.last = 0x8A,
			},
			{
				.first = 0x8C,
				.last = 0x96,
			},
			{
				.first = 0xAA,
				.last = 0xB4,
			},
			{
				.first = 0xB7,
				.last = 0xBF,
			},
			{
				.first = 0xC1,
				.last = 0xCA,
			},
			{
				.first = 0xD3,
				.last = 0xE0,
			},
			{
				.first = 0xF0,
				.last = 0xF8,
			},
		},
	},
	[AB5500_BANK_DBI_ECI] = {
		.nranges = 3,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x07,
			},
			{
				.first = 0x10,
				.last = 0x10,
			},
			{
				.first = 0x13,
				.last = 0x13,
			},
		},
	},
	[AB5500_BANK_CHG] = {
		.nranges = 1,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x11,
				.last = 0x1B,
			},
		},
	},
	[AB5500_BANK_FG_BATTCOM_ACC] = {
		.nranges = 5,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x10,
			},
			{
				.first = 0x1A,
				.last = 0x1D,
			},
			{
				.first = 0x20,
				.last = 0x21,
			},
			{
				.first = 0x23,
				.last = 0x24,
			},
			{
				.first = 0xFC,
				.last = 0xFE,
			},
		},
	},
	[AB5500_BANK_USB] = {
		.nranges = 13,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x01,
				.last = 0x01,
			},
			{
				.first = 0x80,
				.last = 0x83,
			},
			{
				.first = 0x87,
				.last = 0x8B,
			},
			{
				.first = 0x91,
				.last = 0x94,
			},
			{
				.first = 0xA8,
				.last = 0xB0,
			},
			{
				.first = 0xB2,
				.last = 0xB2,
			},
			{
				.first = 0xB4,
				.last = 0xBC,
			},
			{
				.first = 0xBF,
				.last = 0xBF,
			},
			{
				.first = 0xC1,
				.last = 0xC6,
			},
			{
				.first = 0xCD,
				.last = 0xCD,
			},
			{
				.first = 0xD6,
				.last = 0xDA,
			},
			{
				.first = 0xDC,
				.last = 0xDC,
			},
			{
				.first = 0xE0,
				.last = 0xE4,
			},
		},
	},
	[AB5500_BANK_IT] = {
		.nranges = 4,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x02,
			},
			{
				.first = 0x20,
				.last = 0x36,
			},
			{
				.first = 0x60,
				.last = 0x76,
			},
			{
				.first = 0x80,
				.last = 0x80,
			},
		},
	},
	[AB5500_BANK_VDDDIG_IO_I2C_CLK_TST] = {
		.nranges = 7,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x02,
				.last = 0x02,
			},
			{
				.first = 0x12,
				.last = 0x12,
			},
			{
				.first = 0x30,
				.last = 0x34,
			},
			{
				.first = 0x40,
				.last = 0x44,
			},
			{
				.first = 0x50,
				.last = 0x54,
			},
			{
				.first = 0x60,
				.last = 0x64,
			},
			{
				.first = 0x70,
				.last = 0x74,
			},
		},
	},
	[AB5500_BANK_VIT_IO_I2C_CLK_TST_OTP] = {
		.nranges = 12,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x01,
				.last = 0x02,
			},
			{
				.first = 0x0D,
				.last = 0x0F,
			},
			{
				.first = 0x1C,
				.last = 0x1C,
			},
			{
				.first = 0x1E,
				.last = 0x1E,
			},
			{
				.first = 0x20,
				.last = 0x21,
			},
			{
				.first = 0x25,
				.last = 0x25,
			},
			{
				.first = 0x28,
				.last = 0x2A,
			},
			{
				.first = 0x30,
				.last = 0x33,
			},
			{
				.first = 0x40,
				.last = 0x43,
			},
			{
				.first = 0x50,
				.last = 0x53,
			},
			{
				.first = 0x60,
				.last = 0x63,
			},
			{
				.first = 0x70,
				.last = 0x73,
			},
		},
	},
	[AB5500_BANK_VIBRA] = {
		.nranges = 2,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x10,
				.last = 0x13,
			},
			{
				.first = 0xFE,
				.last = 0xFE,
			},
		},
	},
	[AB5500_BANK_AUDIO_HEADSETUSB] = {
		.nranges = 2,
		.range = (struct ab5500_reg_range[]) {
			{
				.first = 0x00,
				.last = 0x48,
			},
			{
				.first = 0xEB,
				.last = 0xFB,
			},
		},
	},
};

static int ab5500_registers_print(struct seq_file *s, void *p)
{
	struct ab5500 *ab = s->private;
	unsigned int i;
	u8 bank = (u8)ab->debug_bank;

	seq_printf(s, AB5500_NAME_STRING " register values:\n");

	seq_printf(s, " bank %u, %s (0x%x):\n", bank,
		bankinfo[bank].name,
		bankinfo[bank].slave_addr);
	for (i = 0; i < debug_ranges[bank].nranges; i++) {
		u8 reg;
		int err;

		for (reg = debug_ranges[bank].range[i].first;
			reg <= debug_ranges[bank].range[i].last;
			reg++) {
			u8 value;

			err = get_register_interruptible(ab, bank, reg,
				&value);
			if (err < 0) {
				dev_err(ab->dev, "get_reg failed %d, bank 0x%x"
					", reg 0x%x\n", err, bank, reg);
				return err;
			}

			err = seq_printf(s, "  [%d/0x%02X]: 0x%02X\n", bank,
				reg, value);
			if (err < 0) {
				dev_err(ab->dev, "seq_printf overflow\n");
				/*
				 * Error is not returned here since
				 * the output is wanted in any case
				 */
				return 0;
			}
		}
	}
	return 0;
}

static int ab5500_registers_open(struct inode *inode, struct file *file)
{
	return single_open(file, ab5500_registers_print, inode->i_private);
}

static const struct file_operations ab5500_registers_fops = {
	.open = ab5500_registers_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int ab5500_bank_print(struct seq_file *s, void *p)
{
	struct ab5500 *ab = s->private;

	seq_printf(s, "%d\n", ab->debug_bank);
	return 0;
}

static int ab5500_bank_open(struct inode *inode, struct file *file)
{
	return single_open(file, ab5500_bank_print, inode->i_private);
}

static ssize_t ab5500_bank_write(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct ab5500 *ab = ((struct seq_file *)(file->private_data))->private;
	char buf[32];
	int buf_size;
	unsigned long user_bank;
	int err;

	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	err = strict_strtoul(buf, 0, &user_bank);
	if (err)
		return -EINVAL;

	if (user_bank >= AB5500_NUM_BANKS) {
		dev_err(ab->dev,
			"debugfs error input > number of banks\n");
		return -EINVAL;
	}

	ab->debug_bank = user_bank;

	return buf_size;
}

static int ab5500_address_print(struct seq_file *s, void *p)
{
	struct ab5500 *ab = s->private;

	seq_printf(s, "0x%02X\n", ab->debug_address);
	return 0;
}

static int ab5500_address_open(struct inode *inode, struct file *file)
{
	return single_open(file, ab5500_address_print, inode->i_private);
}

static ssize_t ab5500_address_write(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct ab5500 *ab = ((struct seq_file *)(file->private_data))->private;
	char buf[32];
	int buf_size;
	unsigned long user_address;
	int err;

	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	err = strict_strtoul(buf, 0, &user_address);
	if (err)
		return -EINVAL;
	if (user_address > 0xff) {
		dev_err(ab->dev,
			"debugfs error input > 0xff\n");
		return -EINVAL;
	}
	ab->debug_address = user_address;
	return buf_size;
}

static int ab5500_val_print(struct seq_file *s, void *p)
{
	struct ab5500 *ab = s->private;
	int err;
	u8 regvalue;

	err = get_register_interruptible(ab, (u8)ab->debug_bank,
		(u8)ab->debug_address, &regvalue);
	if (err) {
		dev_err(ab->dev, "get_reg failed %d, bank 0x%x"
			", reg 0x%x\n", err, ab->debug_bank,
			ab->debug_address);
		return -EINVAL;
	}
	seq_printf(s, "0x%02X\n", regvalue);

	return 0;
}

static int ab5500_val_open(struct inode *inode, struct file *file)
{
	return single_open(file, ab5500_val_print, inode->i_private);
}

static ssize_t ab5500_val_write(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct ab5500 *ab = ((struct seq_file *)(file->private_data))->private;
	char buf[32];
	int buf_size;
	unsigned long user_val;
	int err;
	u8 regvalue;

	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	err = strict_strtoul(buf, 0, &user_val);
	if (err)
		return -EINVAL;
	if (user_val > 0xff) {
		dev_err(ab->dev,
			"debugfs error input > 0xff\n");
		return -EINVAL;
	}
	err = mask_and_set_register_interruptible(
		ab, (u8)ab->debug_bank,
		(u8)ab->debug_address, 0xFF, (u8)user_val);
	if (err)
		return -EINVAL;

	get_register_interruptible(ab, (u8)ab->debug_bank,
		(u8)ab->debug_address, &regvalue);
	if (err)
		return -EINVAL;

	return buf_size;
}

static const struct file_operations ab5500_bank_fops = {
	.open = ab5500_bank_open,
	.write = ab5500_bank_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ab5500_address_fops = {
	.open = ab5500_address_open,
	.write = ab5500_address_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations ab5500_val_fops = {
	.open = ab5500_val_open,
	.write = ab5500_val_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static struct dentry *ab5500_dir;
static struct dentry *ab5500_reg_file;
static struct dentry *ab5500_bank_file;
static struct dentry *ab5500_address_file;
static struct dentry *ab5500_val_file;

static inline void ab5500_setup_debugfs(struct ab5500 *ab)
{
	ab->debug_bank = AB5500_BANK_VIT_IO_I2C_CLK_TST_OTP;
	ab->debug_address = AB5500_CHIP_ID;

	ab5500_dir = debugfs_create_dir(AB5500_NAME_STRING, NULL);
	if (!ab5500_dir)
		goto exit_no_debugfs;

	ab5500_reg_file = debugfs_create_file("all-bank-registers",
		S_IRUGO, ab5500_dir, ab, &ab5500_registers_fops);
	if (!ab5500_reg_file)
		goto exit_destroy_dir;

	ab5500_bank_file = debugfs_create_file("register-bank",
		(S_IRUGO | S_IWUGO), ab5500_dir, ab, &ab5500_bank_fops);
	if (!ab5500_bank_file)
		goto exit_destroy_reg;

	ab5500_address_file = debugfs_create_file("register-address",
		(S_IRUGO | S_IWUGO), ab5500_dir, ab, &ab5500_address_fops);
	if (!ab5500_address_file)
		goto exit_destroy_bank;

	ab5500_val_file = debugfs_create_file("register-value",
		(S_IRUGO | S_IWUGO), ab5500_dir, ab, &ab5500_val_fops);
	if (!ab5500_val_file)
		goto exit_destroy_address;

	return;

exit_destroy_address:
	debugfs_remove(ab5500_address_file);
exit_destroy_bank:
	debugfs_remove(ab5500_bank_file);
exit_destroy_reg:
	debugfs_remove(ab5500_reg_file);
exit_destroy_dir:
	debugfs_remove(ab5500_dir);
exit_no_debugfs:
	dev_err(ab->dev, "failed to create debugfs entries.\n");
	return;
}

static inline void ab5500_remove_debugfs(void)
{
	debugfs_remove(ab5500_val_file);
	debugfs_remove(ab5500_address_file);
	debugfs_remove(ab5500_bank_file);
	debugfs_remove(ab5500_reg_file);
	debugfs_remove(ab5500_dir);
}

#else /* !CONFIG_DEBUG_FS */
static inline void ab5500_setup_debugfs(struct ab5500 *ab)
{
}
static inline void ab5500_remove_debugfs(void)
{
}
#endif

/*
 * Basic set-up, datastructure creation/destruction and I2C interface.
 * This sets up a default config in the AB5500 chip so that it
 * will work as expected.
 */
static int __init ab5500_setup(struct ab5500 *ab,
	struct abx500_init_settings *settings, unsigned int size)
{
	int err = 0;
	int i;

	for (i = 0; i < size; i++) {
		err = mask_and_set_register_interruptible(ab,
			settings[i].bank,
			settings[i].reg,
			0xFF, settings[i].setting);
		if (err)
			goto exit_no_setup;

		/* If event mask register update the event mask in ab5500 */
		if ((settings[i].bank == AB5500_BANK_IT) &&
			(AB5500_MASK_BASE <= settings[i].reg) &&
			(settings[i].reg <= AB5500_MASK_END)) {
			ab->event_mask[settings[i].reg - AB5500_MASK_BASE] =
				settings[i].setting;
		}
	}
exit_no_setup:
	return err;
}

static void ab5500_mask_work(struct work_struct *work)
{
	struct ab5500 *ab = container_of(work, struct ab5500, mask_work);
	int i;
	int err;
	unsigned long flags;
	u8 mask[AB5500_NUM_EVENT_REG];
	int call_prcmu_event_readout = 0;

	spin_lock_irqsave(&ab->event_lock, flags);
	for (i = 0; i < AB5500_NUM_EVENT_REG; i++)
		mask[i] = ab->event_mask[i];
	spin_unlock_irqrestore(&ab->event_lock, flags);

	for (i = 0; i < AB5500_NUM_EVENT_REG; i++) {
		if (mask[i] != ab->last_event_mask[i]) {
			err = mask_and_set_register_interruptible(ab, 0,
				(AB5500_MASK_BASE + i), ~0, mask[i]);
			if (err) {
				dev_err(ab->dev,
					"ab5500_mask_work failed 0x%x,0x%x\n",
					(AB5500_MASK_BASE + i), mask[i]);
				break;
			}

			if (mask[i] == 0xFF) {
				ab->abb_events &= ~BIT(i);
				call_prcmu_event_readout = 1;
			} else {
				ab->abb_events |= BIT(i);
				if (ab->last_event_mask[i] == 0xFF)
					call_prcmu_event_readout = 1;
			}

			ab->last_event_mask[i] = mask[i];
		}
	}
	if (call_prcmu_event_readout) {
		err = db5500_prcmu_config_abb_event_readout(ab->abb_events);
		if (err)
			dev_err(ab->dev,
				"prcmu_config_abb_event_readout failed\n");
	}
}

static void ab5500_mask(struct irq_data *data)
{
	unsigned long flags;
	struct ab5500 *ab;
 	int irq;
 
 	ab = irq_data_get_irq_chip_data(data);
	irq = data->irq - ab->irq.base;

 	spin_lock_irqsave(&ab->event_lock, flags);
	ab->event_mask[irq / 8] |= BIT(irq % 8);
	spin_unlock_irqrestore(&ab->event_lock, flags);

	schedule_work(&ab->mask_work);
}

static void ab5500_unmask(struct irq_data *data)
{
	unsigned long flags;
	struct ab5500 *ab;
 	int irq;
 
 	ab = irq_data_get_irq_chip_data(data);
	irq = data->irq - ab->irq.base;

	spin_lock_irqsave(&ab->event_lock, flags);
	ab->event_mask[irq / 8] &= ~BIT(irq % 8);
	spin_unlock_irqrestore(&ab->event_lock, flags);

	schedule_work(&ab->mask_work);
}

static void noop(unsigned int irq)
{
}

static struct irq_chip ab5500_irq_chip = {
	.name		= "ab5500-core", /* Keep the same name as the request */
	.startup	= NULL, /* defaults to enable */
	.shutdown	= NULL, /* defaults to disable */
	.enable		= NULL, /* defaults to unmask */
	.disable	= ab5500_mask, /* No default to mask in chip.c */
	.ack		= noop,
	.mask		= ab5500_mask,
	.unmask		= ab5500_unmask,
	.end		= NULL,
};

struct ab_family_id {
	u8	id;
	char	*name;
};

static const struct ab_family_id ids[] __initdata = {
	/* AB5500 */
	{
		.id = AB5500_1_0,
		.name = "1.0"
	},
	/* Terminator */
	{
		.id = 0x00,
	}
};

static int __init ab5500_probe(struct platform_device *pdev)
{
	struct ab5500 *ab;
	struct ab5500_platform_data *ab5500_plf_data =
		pdev->dev.platform_data;
	struct resource *res;
	int err;
	int i;

	ab = kzalloc(sizeof(struct ab5500), GFP_KERNEL);
	if (!ab) {
		dev_err(&pdev->dev,
			"could not allocate " AB5500_NAME_STRING " device\n");
		return -ENOMEM;
	}

	/* Initialize data structure */
	mutex_init(&ab->access_mutex);
	spin_lock_init(&ab->event_lock);
	ab->dev = &pdev->dev;
	ab->irq_base = ab5500_plf_data->irq.base;

	platform_set_drvdata(pdev, ab);

	/* Read chip ID register */
	err = get_register_interruptible(ab, AB5500_BANK_VIT_IO_I2C_CLK_TST_OTP,
		AB5500_CHIP_ID, &ab->chip_id);
	if (err) {
		dev_err(&pdev->dev, "could not communicate with the analog "
			"baseband chip\n");
		goto exit_no_detect;
	}

	for (i = 0; ids[i].id != 0x0; i++) {
		if (ids[i].id == ab->chip_id) {
			snprintf(&ab->chip_name[0], sizeof(ab->chip_name) - 1,
				AB5500_ID_FORMAT_STRING, ids[i].name);
			break;
		}
	}

	if (ids[i].id == 0x0) {
		dev_err(&pdev->dev, "unknown analog baseband chip id: 0x%x\n",
			ab->chip_id);
		dev_err(&pdev->dev, "driver not started!\n");
		goto exit_no_detect;
	}

	dev_info(&pdev->dev, "detected AB chip: %s\n", &ab->chip_name[0]);

	/* Readout ab->starup_events when prcmu driver is in place */
	ab->startup_events[0] = 0;

	err = ab5500_setup(ab, ab5500_plf_data->init_settings,
		ab5500_plf_data->init_settings_sz);
	if (err) {
		dev_err(&pdev->dev, "ab5500_setup error\n");
		goto exit_no_setup;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	INIT_WORK(&ab->irq_work, ab5500_irq_work);
#endif
	INIT_WORK(&ab->mask_work, ab5500_mask_work);

	for (i = 0; i < ab5500_plf_data->irq.count; i++) {
		unsigned int irq;

		irq = ab5500_plf_data->irq.base + i;
		set_irq_chip_data(irq, ab);
		set_irq_chip_and_handler(irq, &ab5500_irq_chip,
			handle_simple_irq);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
		set_irq_nested_thread(irq, 1);
#endif
		set_irq_flags(irq, IRQF_VALID);
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	if (!res) {
		dev_err(&pdev->dev, "ab5500_platform_get_resource error\n");
		goto exit_no_irq;
	}
	ab->ab5500_irq = res->start;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	/* This really unpredictable IRQ is of course sampled for entropy. */
	err = request_irq(res->start, ab5500_irq_handler,
		(IRQF_DISABLED | IRQF_SAMPLE_RANDOM), "ab5500-core", ab);
	if (err) {
		dev_err(&pdev->dev, "ab5500_request_irq error\n");
		goto exit_no_irq;
	}

	/* We probably already got an irq here, but if not,
	 * we force a first time and save the startup events here.*/
	disable_irq_nosync(res->start);
	schedule_work(&ab->irq_work);
#else
	err = request_threaded_irq(res->start, ab5500_irq_handler, NULL,
		IRQF_SAMPLE_RANDOM, "ab5500-core", ab);
	/* This real unpredictable IRQ is of course sampled for entropy */
	rand_initialize_irq(res->start);

	if (err) {
		dev_err(&pdev->dev, "ab5500_request_irq error\n");
		goto exit_no_irq;
	}
#endif

	err = abx500_register_ops(&pdev->dev, &ab5500_ops);
	if (err) {
		dev_err(&pdev->dev, "ab5500_register ops error\n");
		goto exit_no_ops;
	}

	/* Set up and register the platform devices. */
	for (i = 0; i < AB5500_NUM_DEVICES; i++) {
		ab5500_devs[i].platform_data = ab5500_plf_data->dev_data[i];
		ab5500_devs[i].data_size = ab5500_plf_data->dev_data_sz[i];
	}

	err = mfd_add_devices(&pdev->dev, 0, ab5500_devs,
		ARRAY_SIZE(ab5500_devs), NULL,
		ab5500_plf_data->irq.base);
	if (err) {
		dev_err(&pdev->dev, "ab5500_mfd_add_device error\n");
		goto exit_no_ops;
	}

	ab5500_setup_debugfs(ab);

	return 0;

exit_no_ops:
exit_no_irq:
exit_no_setup:
exit_no_detect:
	kfree(ab);
	return err;
}

static int __exit ab5500_remove(struct platform_device *pdev)
{
	struct ab5500 *ab = platform_get_drvdata(pdev);
	struct resource *res;

	/*
	 * At this point, all subscribers should have unregistered
	 * their notifiers so deactivate IRQ
	 */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	free_irq(res->start, ab);

	mfd_remove_devices(&pdev->dev);
	ab5500_remove_debugfs();

	kfree(ab);
	return 0;
}

static struct platform_driver ab5500_driver = {
	.driver = {
		.name = "ab5500-core",
		.owner = THIS_MODULE,
	},
	.remove  = __exit_p(ab5500_remove),
};

static int __init ab5500_core_init(void)
{
	return platform_driver_probe(&ab5500_driver, ab5500_probe);
}

static void __exit ab5500_core_exit(void)
{
	platform_driver_unregister(&ab5500_driver);
}

subsys_initcall(ab5500_core_init);
module_exit(ab5500_core_exit);

MODULE_AUTHOR("Mattias Wallin <mattias.wallin@stericsson.com>");
MODULE_DESCRIPTION("AB5500 core driver");
MODULE_LICENSE("GPL");
