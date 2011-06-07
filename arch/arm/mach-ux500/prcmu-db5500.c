/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Mattias Nilsson <mattias.i.nilsson@stericsson.com>
 *
 * U5500 PRCM Unit interface driver
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/prcmu.h>
#include <mach/db5500-regs.h>

#include "prcmu-regs-db5500.h"

#define _PRCM_MB_HEADER (tcdm_base + 0xFE8)
#define PRCM_REQ_MB0_HEADER (_PRCM_MB_HEADER + 0x0)
#define PRCM_REQ_MB1_HEADER (_PRCM_MB_HEADER + 0x1)
#define PRCM_REQ_MB2_HEADER (_PRCM_MB_HEADER + 0x2)
#define PRCM_REQ_MB3_HEADER (_PRCM_MB_HEADER + 0x3)
#define PRCM_REQ_MB4_HEADER (_PRCM_MB_HEADER + 0x4)
#define PRCM_REQ_MB5_HEADER (_PRCM_MB_HEADER + 0x5)
#define PRCM_REQ_MB6_HEADER (_PRCM_MB_HEADER + 0x6)
#define PRCM_REQ_MB7_HEADER (_PRCM_MB_HEADER + 0x7)
#define PRCM_ACK_MB0_HEADER (_PRCM_MB_HEADER + 0x8)
#define PRCM_ACK_MB1_HEADER (_PRCM_MB_HEADER + 0x9)
#define PRCM_ACK_MB2_HEADER (_PRCM_MB_HEADER + 0xa)
#define PRCM_ACK_MB3_HEADER (_PRCM_MB_HEADER + 0xb)
#define PRCM_ACK_MB4_HEADER (_PRCM_MB_HEADER + 0xc)
#define PRCM_ACK_MB5_HEADER (_PRCM_MB_HEADER + 0xd)
#define PRCM_ACK_MB6_HEADER (_PRCM_MB_HEADER + 0xe)
#define PRCM_ACK_MB7_HEADER (_PRCM_MB_HEADER + 0xf)

/* Req Mailboxes */
#define PRCM_REQ_MB0 (tcdm_base + 0xFD8)
#define PRCM_REQ_MB1 (tcdm_base + 0xFCC)
#define PRCM_REQ_MB2 (tcdm_base + 0xFC4)
#define PRCM_REQ_MB3 (tcdm_base + 0xFC0)
#define PRCM_REQ_MB4 (tcdm_base + 0xF98)
#define PRCM_REQ_MB5 (tcdm_base + 0xF90)
#define PRCM_REQ_MB6 (tcdm_base + 0xF8C)
#define PRCM_REQ_MB7 (tcdm_base + 0xF84)

/* Ack Mailboxes */
#define PRCM_ACK_MB0 (tcdm_base + 0xF38)
#define PRCM_ACK_MB1 (tcdm_base + 0xF30)
#define PRCM_ACK_MB2 (tcdm_base + 0xF24)
#define PRCM_ACK_MB3 (tcdm_base + 0xF20)
#define PRCM_ACK_MB4 (tcdm_base + 0xF1C)
#define PRCM_ACK_MB5 (tcdm_base + 0xF14)
#define PRCM_ACK_MB6 (tcdm_base + 0xF0C)
#define PRCM_ACK_MB7 (tcdm_base + 0xF08)

/* Mailbox 2 REQs */
#define PRCM_REQ_MB2_EPOD_CLIENT (PRCM_REQ_MB2 + 0x0)
#define PRCM_REQ_MB2_EPOD_STATE  (PRCM_REQ_MB2 + 0x1)
#define PRCM_REQ_MB2_CLK_CLIENT  (PRCM_REQ_MB2 + 0x2)
#define PRCM_REQ_MB2_CLK_STATE   (PRCM_REQ_MB2 + 0x3)
#define PRCM_REQ_MB2_PLL_CLIENT  (PRCM_REQ_MB2 + 0x4)
#define PRCM_REQ_MB2_PLL_STATE   (PRCM_REQ_MB2 + 0x5)

/* Mailbox 2 ACKs */
#define PRCM_ACK_MB2_EPOD_STATUS (PRCM_ACK_MB2 + 0x2)
#define PRCM_ACK_MB2_CLK_STATUS  (PRCM_ACK_MB2 + 0x6)
#define PRCM_ACK_MB2_PLL_STATUS  (PRCM_ACK_MB2 + 0xA)

enum mb_return_code {
	RC_SUCCESS,
	RC_FAIL,
};

/* Mailbox 0 headers. */
enum mb0_header {
	/* request */
	RMB0H_PWR_STATE_TRANS = 1,
	RMB0H_WAKE_UP_CFG,
	RMB0H_RD_WAKE_UP_ACK,
	/* acknowledge */
	AMB0H_WAKE_UP = 1,
};

/* Mailbox 2 headers. */
enum mb2_header {
	MB2H_EPOD_REQUEST = 1,
	MB2H_CLK_REQUEST,
	MB2H_PLL_REQUEST,
};

/* Mailbox 5 headers. */
enum mb5_header {
	MB5H_I2C_WRITE = 1,
	MB5H_I2C_READ,
};

enum epod_state {
	EPOD_OFF,
	EPOD_ON,
};
enum db5500_prcmu_pll {
	DB5500_PLL_SOC0,
	DB5500_PLL_SOC1,
	DB5500_PLL_DDR,
	DB5500_NUM_PLL_ID,
};

/* Request mailbox 5 fields. */
#define PRCM_REQ_MB5_I2C_SLAVE (PRCM_REQ_MB5 + 0)
#define PRCM_REQ_MB5_I2C_REG (PRCM_REQ_MB5 + 1)
#define PRCM_REQ_MB5_I2C_SIZE (PRCM_REQ_MB5 + 2)
#define PRCM_REQ_MB5_I2C_DATA (PRCM_REQ_MB5 + 4)

/* Acknowledge mailbox 5 fields. */
#define PRCM_ACK_MB5_RETURN_CODE (PRCM_ACK_MB5 + 0)
#define PRCM_ACK_MB5_I2C_DATA (PRCM_ACK_MB5 + 4)

#define NUM_MB 8
#define MBOX_BIT BIT
#define ALL_MBOX_BITS (MBOX_BIT(NUM_MB) - 1)

/*
* Used by MCDE to setup all necessary PRCMU registers
*/
#define PRCMU_RESET_DSIPLL			0x00004000
#define PRCMU_UNCLAMP_DSIPLL			0x00400800

/* HDMI CLK MGT PLLSW=001 (PLLSOC0), PLLDIV=0x8, = 50 Mhz*/
#define PRCMU_DSI_CLOCK_SETTING			0x00000128
/* TVCLK_MGT PLLSW=001 (PLLSOC0) PLLDIV=0x13, = 19.05 MHZ */
#define PRCMU_DSI_LP_CLOCK_SETTING		0x00000135
#define PRCMU_PLLDSI_FREQ_SETTING		0x00020121
#define PRCMU_DSI_PLLOUT_SEL_SETTING		0x00000002
#define PRCMU_ENABLE_ESCAPE_CLOCK_DIV		0x03000201
#define PRCMU_DISABLE_ESCAPE_CLOCK_DIV		0x00000101

#define PRCMU_ENABLE_PLLDSI			0x00000001
#define PRCMU_DISABLE_PLLDSI			0x00000000

#define PRCMU_DSI_RESET_SW			0x00000003
#define PRCMU_RESOUTN0_PIN			0x00000001
#define PRCMU_RESOUTN1_PIN			0x00000002
#define PRCMU_RESOUTN2_PIN			0x00000004

#define PRCMU_PLLDSI_LOCKP_LOCKED		0x3

/*
 * mb0_transfer - state needed for mailbox 0 communication.
 * @lock:		The transaction lock.
 */
static struct {
	spinlock_t lock;
} mb0_transfer;

/*
 * mb2_transfer - state needed for mailbox 2 communication.
 * @lock:      The transaction lock.
 * @work:      The transaction completion structure.
 * @req:       Request data that need to persist between requests.
 * @ack:       Reply ("acknowledge") data.
 */
static struct {
	struct mutex lock;
	struct completion work;
	struct {
		u8 epod_states[DB5500_NUM_EPOD_ID];
		u8 pll_states[DB5500_NUM_PLL_ID];
	} req;
	struct {
		u8 header;
		u8 status;
	} ack;
} mb2_transfer;

/*
 * mb5_transfer - state needed for mailbox 5 communication.
 * @lock:	The transaction lock.
 * @work:	The transaction completion structure.
 * @ack:	Reply ("acknowledge") data.
 */
static struct {
	struct mutex lock;
	struct completion work;
	struct {
		u8 header;
		u8 status;
		u8 value[4];
	} ack;
} mb5_transfer;

/* PRCMU TCDM base IO address. */
static __iomem void *tcdm_base;

struct clk_mgt {
	unsigned int offset;
	u32 pllsw;
};

static DEFINE_SPINLOCK(clk_mgt_lock);

#define CLK_MGT_ENTRY(_name)[PRCMU_##_name] = {	\
	(DB5500_PRCM_##_name##_MGT), 0			\
}
static struct clk_mgt clk_mgt[PRCMU_NUM_REG_CLOCKS] = {
	CLK_MGT_ENTRY(SGACLK),
	CLK_MGT_ENTRY(UARTCLK),
	CLK_MGT_ENTRY(MSP02CLK),
	CLK_MGT_ENTRY(I2CCLK),
	CLK_MGT_ENTRY(SDMMCCLK),
	CLK_MGT_ENTRY(PER1CLK),
	CLK_MGT_ENTRY(PER2CLK),
	CLK_MGT_ENTRY(PER3CLK),
	CLK_MGT_ENTRY(PER5CLK),
	CLK_MGT_ENTRY(PER6CLK),
	CLK_MGT_ENTRY(PWMCLK),
	CLK_MGT_ENTRY(IRDACLK),
	CLK_MGT_ENTRY(IRRCCLK),
	CLK_MGT_ENTRY(HDMICLK),
	CLK_MGT_ENTRY(APEATCLK),
	CLK_MGT_ENTRY(APETRACECLK),
	CLK_MGT_ENTRY(MCDECLK),
	CLK_MGT_ENTRY(DSIALTCLK),
	CLK_MGT_ENTRY(DMACLK),
	CLK_MGT_ENTRY(B2R2CLK),
	CLK_MGT_ENTRY(TVCLK),
	CLK_MGT_ENTRY(RNGCLK),
	CLK_MGT_ENTRY(SIACLK),
	CLK_MGT_ENTRY(SVACLK),
};

static int request_timclk(bool enable)
{
	u32 val = (PRCM_TCR_DOZE_MODE | PRCM_TCR_TENSEL_MASK);

	if (!enable)
		val |= PRCM_TCR_STOP_TIMERS;
	writel(val, (_PRCMU_BASE + PRCM_TCR));

	return 0;
}

static int request_reg_clock(u8 clock, bool enable)
{
	u32 val;
	unsigned long flags;

	WARN_ON(!clk_mgt[clock].offset);

	spin_lock_irqsave(&clk_mgt_lock, flags);

	/* Grab the HW semaphore. */
	while ((readl(_PRCMU_BASE + PRCM_SEM) & PRCM_SEM_PRCM_SEM) != 0)
		cpu_relax();

	val = readl(_PRCMU_BASE + clk_mgt[clock].offset);
	if (enable) {
		val |= (PRCM_CLK_MGT_CLKEN | clk_mgt[clock].pllsw);
	} else {
		clk_mgt[clock].pllsw = (val & PRCM_CLK_MGT_CLKPLLSW_MASK);
		val &= ~(PRCM_CLK_MGT_CLKEN | PRCM_CLK_MGT_CLKPLLSW_MASK);
	}
	writel(val, (_PRCMU_BASE + clk_mgt[clock].offset));

	/* Release the HW semaphore. */
	writel(0, (_PRCMU_BASE + PRCM_SEM));

	spin_unlock_irqrestore(&clk_mgt_lock, flags);

	return 0;
}

/*
 * request_pll() - Request for a pll to be enabled or disabled.
 * @pll:        The pll for which the request is made.
 * @enable:     Whether the clock should be enabled (true) or disabled (false).
 *
 * This function should only be used by the clock implementation.
 * Do not use it from any other place!
 */
static int request_pll(u8 pll, bool enable)
{
	int r = 0;

	BUG_ON(pll >= DB5500_NUM_PLL_ID);
	mutex_lock(&mb2_transfer.lock);

	while (readl(_PRCMU_BASE + PRCM_MBOX_CPU_VAL) & MBOX_BIT(2))
		cpu_relax();

	mb2_transfer.req.pll_states[pll] = enable;

	/* fill in mailbox */
	writeb(pll, PRCM_REQ_MB2_PLL_CLIENT);
	writeb(mb2_transfer.req.pll_states[pll], PRCM_REQ_MB2_PLL_STATE);

	writeb(MB2H_PLL_REQUEST, PRCM_REQ_MB2_HEADER);

	writel(MBOX_BIT(2), _PRCMU_BASE + PRCM_MBOX_CPU_SET);
	if (!wait_for_completion_timeout(&mb2_transfer.work,
		msecs_to_jiffies(500))) {
		pr_err("prcmu: set_pll() failed.\n"
			"prcmu: Please check your firmware version.\n");
		r = -EIO;
		WARN(1, "Failed to set pll");
		goto unlock_and_return;
	}
	if (mb2_transfer.ack.status != RC_SUCCESS ||
		mb2_transfer.ack.header != MB2H_PLL_REQUEST)
		r = -EIO;

unlock_and_return:
	mutex_unlock(&mb2_transfer.lock);

	return r;
}

void prcmu_enable_wakeups(u32 wakeups)
{
}

/**
 * prcmu_request_clock() - Request for a clock to be enabled or disabled.
 * @clock:      The clock for which the request is made.
 * @enable:     Whether the clock should be enabled (true) or disabled (false).
 *
 * This function should only be used by the clock implementation.
 * Do not use it from any other place!
 */
int prcmu_request_clock(u8 clock, bool enable)
{
	if (clock < PRCMU_NUM_REG_CLOCKS)
		return request_reg_clock(clock, enable);
	else if (clock == PRCMU_TIMCLK)
		return request_timclk(enable);
	else if (clock == PRCMU_PLLSOC0)
		return request_pll(DB5500_PLL_SOC0, enable);
	else if (clock == PRCMU_PLLSOC1)
		return request_pll(DB5500_PLL_SOC1, enable);
	else if (clock == PRCMU_PLLDDR)
		return request_pll(DB5500_PLL_DDR, enable);
	else if (clock == PRCMU_SYSCLK)
		return -EINVAL;
	else
		return -EINVAL;
}

/**
 * db5500_prcmu_abb_read() - Read register value(s) from the ABB.
 * @slave:	The I2C slave address.
 * @reg:	The (start) register address.
 * @value:	The read out value(s).
 * @size:	The number of registers to read.
 *
 * Reads register value(s) from the ABB.
 * @size has to be <= 4.
 */
int db5500_prcmu_abb_read(u8 slave, u8 reg, u8 *value, u8 size)
{
	int r;

	if ((size < 1) || (4 < size))
		return -EINVAL;

	mutex_lock(&mb5_transfer.lock);

	while (readl(_PRCMU_BASE + PRCM_MBOX_CPU_VAL) & MBOX_BIT(5))
		cpu_relax();
	writeb(slave, PRCM_REQ_MB5_I2C_SLAVE);
	writeb(reg, PRCM_REQ_MB5_I2C_REG);
	writeb(size, PRCM_REQ_MB5_I2C_SIZE);
	writeb(MB5H_I2C_READ, PRCM_REQ_MB5_HEADER);

	writel(MBOX_BIT(5), _PRCMU_BASE + PRCM_MBOX_CPU_SET);
	wait_for_completion(&mb5_transfer.work);

	r = 0;
	if ((mb5_transfer.ack.header == MB5H_I2C_READ) &&
		(mb5_transfer.ack.status == RC_SUCCESS))
		memcpy(value, mb5_transfer.ack.value, (size_t)size);
	else
		r = -EIO;

	mutex_unlock(&mb5_transfer.lock);

	return r;
}

/**
 * db5500_prcmu_abb_write() - Write register value(s) to the ABB.
 * @slave:	The I2C slave address.
 * @reg:	The (start) register address.
 * @value:	The value(s) to write.
 * @size:	The number of registers to write.
 *
 * Writes register value(s) to the ABB.
 * @size has to be <= 4.
 */
int db5500_prcmu_abb_write(u8 slave, u8 reg, u8 *value, u8 size)
{
	int r;

	if ((size < 1) || (4 < size))
		return -EINVAL;

	mutex_lock(&mb5_transfer.lock);

	while (readl(_PRCMU_BASE + PRCM_MBOX_CPU_VAL) & MBOX_BIT(5))
		cpu_relax();
	writeb(slave, PRCM_REQ_MB5_I2C_SLAVE);
	writeb(reg, PRCM_REQ_MB5_I2C_REG);
	writeb(size, PRCM_REQ_MB5_I2C_SIZE);
	memcpy_toio(PRCM_REQ_MB5_I2C_DATA, value, size);
	writeb(MB5H_I2C_WRITE, PRCM_REQ_MB5_HEADER);

	writel(MBOX_BIT(5), _PRCMU_BASE + PRCM_MBOX_CPU_SET);
	wait_for_completion(&mb5_transfer.work);

	if ((mb5_transfer.ack.header == MB5H_I2C_WRITE) &&
		(mb5_transfer.ack.status == RC_SUCCESS))
		r = 0;
	else
		r = -EIO;

	mutex_unlock(&mb5_transfer.lock);

	return r;
}

int prcmu_resetout(u8 resoutn, u8 state)
{
	int offset;
	int pin = -1;

	offset = state > 0 ? PRCM_RESOUTN_SET_OFFSET : PRCM_RESOUTN_CLR_OFFSET;

	switch (resoutn) {
	case 0:
		pin = PRCMU_RESOUTN0_PIN;
		break;
	case 1:
		pin = PRCMU_RESOUTN1_PIN;
		break;
	case 2:
		pin = PRCMU_RESOUTN2_PIN;
	default:
		break;
	}

	if (pin > 0)
		writel(pin, _PRCMU_BASE + offset);
	else
		return -EINVAL;

	return 0;
}

int db5500_prcmu_enable_dsipll(void)
{
	int i;

	/* Enable DSIPLL_RESETN resets */
	writel(PRCMU_RESET_DSIPLL, _PRCMU_BASE + PRCM_APE_RESETN_CLR);
	/* Unclamp DSIPLL in/out */
	writel(PRCMU_UNCLAMP_DSIPLL, _PRCMU_BASE + PRCM_MMIP_LS_CLAMP_CLR);
	/* Set DSI PLL FREQ */
	writel(PRCMU_PLLDSI_FREQ_SETTING, _PRCMU_BASE + PRCM_PLLDSI_FREQ);
	writel(PRCMU_DSI_PLLOUT_SEL_SETTING,
		_PRCMU_BASE + PRCM_DSI_PLLOUT_SEL);
	/* Enable Escape clocks */
	writel(PRCMU_ENABLE_ESCAPE_CLOCK_DIV, _PRCMU_BASE + PRCM_DSITVCLK_DIV);

	/* Start DSI PLL */
	writel(PRCMU_ENABLE_PLLDSI, _PRCMU_BASE + PRCM_PLLDSI_ENABLE);
	/* Reset DSI PLL */
	writel(PRCMU_DSI_RESET_SW, _PRCMU_BASE + PRCM_DSI_SW_RESET);
	for (i = 0; i < 10; i++) {
		if ((readl(_PRCMU_BASE + PRCM_PLLDSI_LOCKP) &
			PRCMU_PLLDSI_LOCKP_LOCKED) == PRCMU_PLLDSI_LOCKP_LOCKED)
			break;
		udelay(100);
	}
	/* Release DSIPLL_RESETN */
	writel(PRCMU_RESET_DSIPLL, _PRCMU_BASE + PRCM_APE_RESETN_SET);
	return 0;
}

int db5500_prcmu_disable_dsipll(void)
{
	/* Disable dsi pll */
	writel(PRCMU_DISABLE_PLLDSI, _PRCMU_BASE + PRCM_PLLDSI_ENABLE);
	/* Disable  escapeclock */
	writel(PRCMU_DISABLE_ESCAPE_CLOCK_DIV,
		_PRCMU_BASE + PRCM_DSITVCLK_DIV);
	return 0;
}

int db5500_prcmu_set_display_clocks(void)
{
	writel(PRCMU_DSI_CLOCK_SETTING, _PRCMU_BASE + DB5500_PRCM_HDMICLK_MGT);
	writel(PRCMU_DSI_LP_CLOCK_SETTING, _PRCMU_BASE + DB5500_PRCM_TVCLK_MGT);
	return 0;
}

static void ack_dbb_wakeup(void)
{
	unsigned long flags;

	spin_lock_irqsave(&mb0_transfer.lock, flags);

	while (readl(_PRCMU_BASE + PRCM_MBOX_CPU_VAL) & MBOX_BIT(0))
		cpu_relax();

	writeb(RMB0H_RD_WAKE_UP_ACK, PRCM_REQ_MB0_HEADER);
	writel(MBOX_BIT(0), _PRCMU_BASE + PRCM_MBOX_CPU_SET);

	spin_unlock_irqrestore(&mb0_transfer.lock, flags);
}

int prcmu_set_epod(u16 epod_id, u8 epod_state)
{
	int r = 0;
	bool ram_retention = false;

	/* check argument */
	BUG_ON(epod_id < DB5500_EPOD_ID_BASE);
	BUG_ON(epod_state > EPOD_STATE_ON);

	epod_id &= 0xFF;
	BUG_ON(epod_id > DB5500_NUM_EPOD_ID);

	/* TODO: FW does not take retention for ESRAM12?
	   set flag if retention is possible */
	switch (epod_id) {
	case DB5500_EPOD_ID_ESRAM12:
		ram_retention = true;
		break;
	}

	/* check argument */
	/* BUG_ON(epod_state == EPOD_STATE_RAMRET && !ram_retention); */

	/* get lock */
	mutex_lock(&mb2_transfer.lock);

	/* wait for mailbox */
	while (readl(_PRCMU_BASE + PRCM_MBOX_CPU_VAL) & MBOX_BIT(2))
		cpu_relax();

	/* PRCMU FW can only handle on or off */
	if (epod_state == EPOD_STATE_ON)
		mb2_transfer.req.epod_states[epod_id] = EPOD_ON;
	else if (epod_state == EPOD_STATE_OFF)
		mb2_transfer.req.epod_states[epod_id] = EPOD_OFF;
	else {
		r = -EINVAL;
		goto unlock_and_return;
	}

	/* fill in mailbox */
	writeb(epod_id, PRCM_REQ_MB2_EPOD_CLIENT);
	writeb(mb2_transfer.req.epod_states[epod_id], PRCM_REQ_MB2_EPOD_STATE);

	writeb(MB2H_EPOD_REQUEST, PRCM_REQ_MB2_HEADER);

	writel(MBOX_BIT(2), _PRCMU_BASE + PRCM_MBOX_CPU_SET);

	if (!wait_for_completion_timeout(&mb2_transfer.work,
		msecs_to_jiffies(500))) {
		pr_err("prcmu: set_epod() failed.\n"
			"prcmu: Please check your firmware version.\n");
		r = -EIO;
		WARN(1, "Failed to set epod");
		goto unlock_and_return;
	}

	if (mb2_transfer.ack.status != RC_SUCCESS ||
		mb2_transfer.ack.header != MB2H_EPOD_REQUEST)
		r = -EIO;

unlock_and_return:
	mutex_unlock(&mb2_transfer.lock);
	return r;
}

static inline void print_unknown_header_warning(u8 n, u8 header)
{
	pr_warning("prcmu: Unknown message header (%d) in mailbox %d.\n",
		header, n);
}

static bool read_mailbox_0(void)
{
	bool r;
	u8 header;

	header = readb(PRCM_ACK_MB0_HEADER);
	switch (header) {
	case AMB0H_WAKE_UP:
		r = true;
		break;
	default:
		print_unknown_header_warning(0, header);
		r = false;
		break;
	}
	writel(MBOX_BIT(0), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	return r;
}

static bool read_mailbox_1(void)
{
	writel(MBOX_BIT(1), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	return false;
}

static bool read_mailbox_2(void)
{
	u8 header;

	header = readb(PRCM_ACK_MB2_HEADER);
	mb2_transfer.ack.header = header;
	switch (header) {
	case MB2H_EPOD_REQUEST:
		mb2_transfer.ack.status = readb(PRCM_ACK_MB2_EPOD_STATUS);
		break;
	case MB2H_CLK_REQUEST:
		mb2_transfer.ack.status = readb(PRCM_ACK_MB2_CLK_STATUS);
		break;
	case MB2H_PLL_REQUEST:
		mb2_transfer.ack.status = readb(PRCM_ACK_MB2_PLL_STATUS);
		break;
	default:
		writel(MBOX_BIT(2), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
		pr_err("prcmu: Wrong ACK received for MB2 request \n");
		return false;
		break;
	}
	writel(MBOX_BIT(2), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	complete(&mb2_transfer.work);
	return false;
}

static bool read_mailbox_3(void)
{
	writel(MBOX_BIT(3), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	return false;
}

static bool read_mailbox_4(void)
{
	writel(MBOX_BIT(4), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	return false;
}

static bool read_mailbox_5(void)
{
	u8 header;

	header = readb(PRCM_ACK_MB5_HEADER);
	switch (header) {
	case MB5H_I2C_READ:
		memcpy_fromio(mb5_transfer.ack.value, PRCM_ACK_MB5_I2C_DATA, 4);
	case MB5H_I2C_WRITE:
		mb5_transfer.ack.header = header;
		mb5_transfer.ack.status = readb(PRCM_ACK_MB5_RETURN_CODE);
		writel(MBOX_BIT(5), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
		complete(&mb5_transfer.work);
		break;
	default:
		writel(MBOX_BIT(5), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
		print_unknown_header_warning(5, header);
		break;
	}
	return false;
}

static bool read_mailbox_6(void)
{
	writel(MBOX_BIT(6), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	return false;
}

static bool read_mailbox_7(void)
{
	writel(MBOX_BIT(7), _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);
	return false;
}

static bool (* const read_mailbox[NUM_MB])(void) = {
	read_mailbox_0,
	read_mailbox_1,
	read_mailbox_2,
	read_mailbox_3,
	read_mailbox_4,
	read_mailbox_5,
	read_mailbox_6,
	read_mailbox_7
};

static irqreturn_t prcmu_irq_handler(int irq, void *data)
{
	u32 bits;
	u8 n;
	irqreturn_t r;

	bits = (readl(_PRCMU_BASE + PRCM_ARM_IT1_VAL) & ALL_MBOX_BITS);
	if (unlikely(!bits))
		return IRQ_NONE;

	r = IRQ_HANDLED;
	for (n = 0; bits; n++) {
		if (bits & MBOX_BIT(n)) {
			bits -= MBOX_BIT(n);
			if (read_mailbox[n]())
				r = IRQ_WAKE_THREAD;
		}
	}
	return r;
}

static irqreturn_t prcmu_irq_thread_fn(int irq, void *data)
{
	ack_dbb_wakeup();
	return IRQ_HANDLED;
}

void __init db5500_prcmu_early_init(void)
{
	tcdm_base = __io_address(U5500_PRCMU_TCDM_BASE);
	spin_lock_init(&mb0_transfer.lock);
	mutex_init(&mb2_transfer.lock);
	mutex_init(&mb5_transfer.lock);
	init_completion(&mb2_transfer.work);
	init_completion(&mb5_transfer.work);
}

/**
 * prcmu_fw_init - arch init call for the Linux PRCMU fw init logic
 *
 */
int __init db5500_prcmu_init(void)
{
	int r = 0;

	if (ux500_is_svp() || !cpu_is_u5500())
		return -ENODEV;

	/* Clean up the mailbox interrupts after pre-kernel code. */
	writel(ALL_MBOX_BITS, _PRCMU_BASE + PRCM_ARM_IT1_CLEAR);

	r = request_threaded_irq(IRQ_DB5500_PRCMU1, prcmu_irq_handler,
		prcmu_irq_thread_fn, 0, "prcmu", NULL);
	if (r < 0) {
		pr_err("prcmu: Failed to allocate IRQ_DB5500_PRCMU1.\n");
		return -EBUSY;
	}
	return 0;
}

arch_initcall(db5500_prcmu_init);
