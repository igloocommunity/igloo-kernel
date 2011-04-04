/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Sundar Iyer <sundar.iyer@stericsson.com>
 *
 */

#include <linux/io.h>

#include <mach/hardware.h>

#include "context.h"

/*
 * ST-Interconnect context
 */

/* priority, bw limiter register offsets */
#define NODE_HIBW1_ESRAM_IN_0_PRIORITY		0x00
#define NODE_HIBW1_ESRAM_IN_1_PRIORITY		0x04
#define NODE_HIBW1_ESRAM_IN_2_PRIORITY		0x08
#define NODE_HIBW1_ESRAM_IN_0_ARB_1_LIMIT	0x24
#define NODE_HIBW1_ESRAM_IN_0_ARB_2_LIMIT	0x28
#define NODE_HIBW1_ESRAM_IN_0_ARB_3_LIMIT	0x2C
#define NODE_HIBW1_ESRAM_IN_1_ARB_1_LIMIT	0x30
#define NODE_HIBW1_ESRAM_IN_1_ARB_2_LIMIT	0x34
#define NODE_HIBW1_ESRAM_IN_1_ARB_3_LIMIT	0x38
#define NODE_HIBW1_ESRAM_IN_2_ARB_1_LIMIT	0x3C
#define NODE_HIBW1_ESRAM_IN_2_ARB_2_LIMIT	0x40
#define NODE_HIBW1_ESRAM_IN_2_ARB_3_LIMIT	0x44
#define NODE_HIBW1_DDR_IN_0_PRIORITY		0x400
#define NODE_HIBW1_DDR_IN_1_PRIORITY		0x404
#define NODE_HIBW1_DDR_IN_2_PRIORITY		0x408
#define NODE_HIBW1_DDR_IN_0_LIMIT		0x424
#define NODE_HIBW1_DDR_IN_1_LIMIT		0x428
#define NODE_HIBW1_DDR_IN_2_LIMIT		0x42C
#define NODE_HIBW1_DDR_OUT_0_PRIORITY		0x430
#define NODE_HIBW2_ESRAM_IN_0_PRIORITY		0x800
#define NODE_HIBW2_ESRAM_IN_1_PRIORITY		0x804
#define NODE_HIBW2_ESRAM_IN_0_ARB_1_LIMIT	0x818
#define NODE_HIBW2_ESRAM_IN_0_ARB_2_LIMIT	0x81C
#define NODE_HIBW2_ESRAM_IN_0_ARB_3_LIMIT	0x820
#define NODE_HIBW2_ESRAM_IN_1_ARB_1_LIMIT	0x824
#define NODE_HIBW2_ESRAM_IN_1_ARB_2_LIMIT	0x828
#define NODE_HIBW2_ESRAM_IN_1_ARB_3_LIMIT	0x82C
#define NODE_HIBW2_DDR_IN_0_PRIORITY		0xC00
#define NODE_HIBW2_DDR_IN_1_PRIORITY		0xC04
#define NODE_HIBW2_DDR_IN_2_PRIORITY		0xC08
/* only in v1 */
#define NODE_HIBW2_DDR_IN_3_PRIORITY		0xC0C
/* address update between v1 and v2 */
#define NODE_HIBW2_DDR_IN_0_LIMIT_V1		0xC30
#define NODE_HIBW2_DDR_IN_1_LIMIT_V1		0xC34
#define NODE_HIBW2_DDR_IN_0_LIMIT		0xC24
#define NODE_HIBW2_DDR_IN_1_LIMIT		0xC28
/* only in v2 */
#define NODE_HIBW2_DDR_IN_2_LIMIT		0xC2C
#define NODE_HIBW2_DDR_OUT_0_PRIORITY		0xC30
#define NODE_ESRAM0_IN_0_PRIORITY		0X1000
#define NODE_ESRAM0_IN_1_PRIORITY		0X1004
#define NODE_ESRAM0_IN_2_PRIORITY		0X1008
#define NODE_ESRAM0_IN_3_PRIORITY		0X100C
#define NODE_ESRAM0_IN_0_LIMIT			0X1030
#define NODE_ESRAM0_IN_1_LIMIT			0X1034
#define NODE_ESRAM0_IN_2_LIMIT			0X1038
#define NODE_ESRAM0_IN_3_LIMIT			0X103C
/* common */
#define NODE_ESRAM1_2_IN_0_PRIORITY		0x1400
#define NODE_ESRAM1_2_IN_1_PRIORITY		0x1404
#define NODE_ESRAM1_2_IN_2_PRIORITY		0x1408
#define NODE_ESRAM1_2_IN_3_PRIORITY		0x140C
#define NODE_ESRAM1_2_IN_0_ARB_1_LIMIT		0x1430
#define NODE_ESRAM1_2_IN_0_ARB_2_LIMIT		0x1434
#define NODE_ESRAM1_2_IN_1_ARB_1_LIMIT		0x1438
#define NODE_ESRAM1_2_IN_1_ARB_2_LIMIT		0x143C
#define NODE_ESRAM1_2_IN_2_ARB_1_LIMIT		0x1440
#define NODE_ESRAM1_2_IN_2_ARB_2_LIMIT		0x1444
#define NODE_ESRAM1_2_IN_3_ARB_1_LIMIT		0x1448
#define NODE_ESRAM1_2_IN_3_ARB_2_LIMIT		0x144C
#define NODE_ESRAM3_4_IN_0_PRIORITY		0x1800
#define NODE_ESRAM3_4_IN_1_PRIORITY		0x1804
#define NODE_ESRAM3_4_IN_2_PRIORITY		0x1808
#define NODE_ESRAM3_4_IN_3_PRIORITY		0x180C
#define NODE_ESRAM3_4_IN_0_ARB_1_LIMIT		0x1830
#define NODE_ESRAM3_4_IN_0_ARB_2_LIMIT		0x1834
#define NODE_ESRAM3_4_IN_1_ARB_1_LIMIT		0x1838
#define NODE_ESRAM3_4_IN_1_ARB_2_LIMIT		0x183C
#define NODE_ESRAM3_4_IN_2_ARB_1_LIMIT		0x1840
#define NODE_ESRAM3_4_IN_2_ARB_2_LIMIT		0x1844
#define NODE_ESRAM3_4_IN_3_ARB_1_LIMIT		0x1848
#define NODE_ESRAM3_4_IN_3_ARB_2_LIMIT		0x184C

static struct {
	void __iomem *base;
	u32 hibw1_esram_in_pri[3];
	u32 hibw1_esram_in0_arb[3];
	u32 hibw1_esram_in1_arb[3];
	u32 hibw1_esram_in2_arb[3];
	u32 hibw1_ddr_in_prio[3];
	u32 hibw1_ddr_in_limit[3];
	u32 hibw1_ddr_out_prio;

	/* HiBw2 node registers */
	u32 hibw2_esram_in_pri[2];
	u32 hibw2_esram_in0_arblimit[3];
	u32 hibw2_esram_in1_arblimit[3];
	u32 hibw2_ddr_in_prio[4];
	u32 hibw2_ddr_in_limit[4];
	u32 hibw2_ddr_out_prio;

	/* ESRAM node registers */
	u32 esram_in_prio[4];
	u32 esram_in_lim[4];
	u32 esram0_in_prio[4];
	u32 esram0_in_lim[4];
	u32 esram12_in_prio[4];
	u32 esram12_in_arb_lim[8];
	u32 esram34_in_prio[4];
	u32 esram34_in_arb_lim[8];
} context_icn;

/**
 * u8500_context_save_icn() - save ICN context
 *
 */
void u8500_context_save_icn(void)
{

	context_icn.hibw1_esram_in_pri[0] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_0_PRIORITY);
	context_icn.hibw1_esram_in_pri[1] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_1_PRIORITY);
	context_icn.hibw1_esram_in_pri[2] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_2_PRIORITY);

	context_icn.hibw1_esram_in0_arb[0] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_1_LIMIT);
	context_icn.hibw1_esram_in0_arb[1] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_2_LIMIT);
	context_icn.hibw1_esram_in0_arb[2] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_3_LIMIT);

	context_icn.hibw1_esram_in1_arb[0] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_1_LIMIT);
	context_icn.hibw1_esram_in1_arb[1] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_2_LIMIT);
	context_icn.hibw1_esram_in1_arb[2] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_3_LIMIT);

	context_icn.hibw1_esram_in2_arb[0] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_2_ARB_1_LIMIT);
	context_icn.hibw1_esram_in2_arb[1] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_2_ARB_2_LIMIT);
	context_icn.hibw1_esram_in2_arb[2] =
		readb(context_icn.base + NODE_HIBW1_ESRAM_IN_2_ARB_3_LIMIT);

	context_icn.hibw1_ddr_in_prio[0] =
		readb(context_icn.base + NODE_HIBW1_DDR_IN_0_PRIORITY);
	context_icn.hibw1_ddr_in_prio[1] =
		readb(context_icn.base + NODE_HIBW1_DDR_IN_1_PRIORITY);
	context_icn.hibw1_ddr_in_prio[2] =
		readb(context_icn.base + NODE_HIBW1_DDR_IN_2_PRIORITY);

	context_icn.hibw1_ddr_in_limit[0] =
		readb(context_icn.base + NODE_HIBW1_DDR_IN_0_LIMIT);
	context_icn.hibw1_ddr_in_limit[1] =
		readb(context_icn.base + NODE_HIBW1_DDR_IN_1_LIMIT);
	context_icn.hibw1_ddr_in_limit[2] =
		readb(context_icn.base + NODE_HIBW1_DDR_IN_2_LIMIT);

	context_icn.hibw1_ddr_out_prio =
		readb(context_icn.base + NODE_HIBW1_DDR_OUT_0_PRIORITY);

	context_icn.hibw2_esram_in_pri[0] =
		readb(context_icn.base + NODE_HIBW2_ESRAM_IN_0_PRIORITY);
	context_icn.hibw2_esram_in_pri[1] =
		readb(context_icn.base + NODE_HIBW2_ESRAM_IN_1_PRIORITY);

	context_icn.hibw2_esram_in0_arblimit[0] =
		readb(context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_1_LIMIT);
	context_icn.hibw2_esram_in0_arblimit[1] =
		 readb(context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_2_LIMIT);
	context_icn.hibw2_esram_in0_arblimit[2] =
		 readb(context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_3_LIMIT);

	context_icn.hibw2_esram_in1_arblimit[0] =
		readb(context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_1_LIMIT);
	context_icn.hibw2_esram_in1_arblimit[1] =
		readb(context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_2_LIMIT);
	context_icn.hibw2_esram_in1_arblimit[2] =
		readb(context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_3_LIMIT);

	context_icn.hibw2_ddr_in_prio[0] =
		readb(context_icn.base + NODE_HIBW2_DDR_IN_0_PRIORITY);
	context_icn.hibw2_ddr_in_prio[1] =
		readb(context_icn.base + NODE_HIBW2_DDR_IN_1_PRIORITY);
	context_icn.hibw2_ddr_in_prio[2] =
		readb(context_icn.base + NODE_HIBW2_DDR_IN_2_PRIORITY);

	if (cpu_is_u8500v1()) {
		context_icn.hibw2_ddr_in_prio[3] =
			readb(context_icn.base + NODE_HIBW2_DDR_IN_3_PRIORITY);

		context_icn.hibw2_ddr_in_limit[0] =
			readb(context_icn.base + NODE_HIBW2_DDR_IN_0_LIMIT_V1);
		context_icn.hibw2_ddr_in_limit[1] =
			readb(context_icn.base + NODE_HIBW2_DDR_IN_1_LIMIT_V1);
	}

	if (cpu_is_u8500v2()) {
		context_icn.hibw2_ddr_in_limit[0] =
			readb(context_icn.base + NODE_HIBW2_DDR_IN_0_LIMIT);
		context_icn.hibw2_ddr_in_limit[1] =
			readb(context_icn.base + NODE_HIBW2_DDR_IN_1_LIMIT);

		context_icn.hibw2_ddr_in_limit[2] =
			readb(context_icn.base + NODE_HIBW2_DDR_IN_2_LIMIT);

		context_icn.hibw2_ddr_out_prio =
			readb(context_icn.base +
			      NODE_HIBW2_DDR_OUT_0_PRIORITY);

		context_icn.esram0_in_prio[0] =
			readb(context_icn.base + NODE_ESRAM0_IN_0_PRIORITY);
		context_icn.esram0_in_prio[1] =
			readb(context_icn.base + NODE_ESRAM0_IN_1_PRIORITY);
		context_icn.esram0_in_prio[2] =
			readb(context_icn.base + NODE_ESRAM0_IN_2_PRIORITY);
		context_icn.esram0_in_prio[3] =
			readb(context_icn.base + NODE_ESRAM0_IN_3_PRIORITY);

		context_icn.esram0_in_lim[0] =
			readb(context_icn.base + NODE_ESRAM0_IN_0_LIMIT);
		context_icn.esram0_in_lim[1] =
			readb(context_icn.base + NODE_ESRAM0_IN_1_LIMIT);
		context_icn.esram0_in_lim[2] =
			readb(context_icn.base + NODE_ESRAM0_IN_2_LIMIT);
		context_icn.esram0_in_lim[3] =
			readb(context_icn.base + NODE_ESRAM0_IN_3_LIMIT);
	}
	context_icn.esram12_in_prio[0] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_0_PRIORITY);
	context_icn.esram12_in_prio[1] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_1_PRIORITY);
	context_icn.esram12_in_prio[2] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_2_PRIORITY);
	context_icn.esram12_in_prio[3] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_3_PRIORITY);

	context_icn.esram12_in_arb_lim[0] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_0_ARB_1_LIMIT);
	context_icn.esram12_in_arb_lim[1] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_0_ARB_2_LIMIT);
	context_icn.esram12_in_arb_lim[2] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_1_ARB_1_LIMIT);
	context_icn.esram12_in_arb_lim[3] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_1_ARB_2_LIMIT);
	context_icn.esram12_in_arb_lim[4] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_2_ARB_1_LIMIT);
	context_icn.esram12_in_arb_lim[5] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_2_ARB_2_LIMIT);
	context_icn.esram12_in_arb_lim[6] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_3_ARB_1_LIMIT);
	context_icn.esram12_in_arb_lim[7] =
		readb(context_icn.base + NODE_ESRAM1_2_IN_3_ARB_2_LIMIT);

	context_icn.esram34_in_prio[0] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_0_PRIORITY);
	context_icn.esram34_in_prio[1] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_1_PRIORITY);
	context_icn.esram34_in_prio[2] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_2_PRIORITY);
	context_icn.esram34_in_prio[3] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_3_PRIORITY);

	context_icn.esram34_in_arb_lim[0] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_0_ARB_1_LIMIT);
	context_icn.esram34_in_arb_lim[1] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_0_ARB_2_LIMIT);
	context_icn.esram34_in_arb_lim[2] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_1_ARB_1_LIMIT);
	context_icn.esram34_in_arb_lim[3] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_1_ARB_2_LIMIT);
	context_icn.esram34_in_arb_lim[4] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_2_ARB_1_LIMIT);
	context_icn.esram34_in_arb_lim[5] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_2_ARB_2_LIMIT);
	context_icn.esram34_in_arb_lim[6] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_3_ARB_1_LIMIT);
	context_icn.esram34_in_arb_lim[7] =
		readb(context_icn.base + NODE_ESRAM3_4_IN_3_ARB_2_LIMIT);

}

/**
 * u8500_context_restore_icn() - restore ICN context
 *
 */
void u8500_context_restore_icn(void)
{
	writel(context_icn.hibw1_esram_in_pri[0],
		context_icn.base + NODE_HIBW1_ESRAM_IN_0_PRIORITY);
	writel(context_icn.hibw1_esram_in_pri[1],
		context_icn.base + NODE_HIBW1_ESRAM_IN_1_PRIORITY);
	writel(context_icn.hibw1_esram_in_pri[2],
		context_icn.base + NODE_HIBW1_ESRAM_IN_2_PRIORITY);

	writel(context_icn.hibw1_esram_in0_arb[0],
		context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_1_LIMIT);
	writel(context_icn.hibw1_esram_in0_arb[1],
		context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_2_LIMIT);
	writel(context_icn.hibw1_esram_in0_arb[2],
		context_icn.base + NODE_HIBW1_ESRAM_IN_0_ARB_3_LIMIT);

	writel(context_icn.hibw1_esram_in1_arb[0],
		context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_1_LIMIT);
	writel(context_icn.hibw1_esram_in1_arb[1],
		context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_2_LIMIT);
	writel(context_icn.hibw1_esram_in1_arb[2],
		context_icn.base + NODE_HIBW1_ESRAM_IN_1_ARB_3_LIMIT);

	writel(context_icn.hibw1_esram_in2_arb[0],
		context_icn.base + NODE_HIBW1_ESRAM_IN_2_ARB_1_LIMIT);
	writel(context_icn.hibw1_esram_in2_arb[1],
		context_icn.base + NODE_HIBW1_ESRAM_IN_2_ARB_2_LIMIT);
	writel(context_icn.hibw1_esram_in2_arb[2],
		context_icn.base + NODE_HIBW1_ESRAM_IN_2_ARB_3_LIMIT);

	writel(context_icn.hibw1_ddr_in_prio[0],
		context_icn.base + NODE_HIBW1_DDR_IN_0_PRIORITY);
	writel(context_icn.hibw1_ddr_in_prio[1],
		context_icn.base + NODE_HIBW1_DDR_IN_1_PRIORITY);
	writel(context_icn.hibw1_ddr_in_prio[2],
		context_icn.base + NODE_HIBW1_DDR_IN_2_PRIORITY);

	writel(context_icn.hibw1_ddr_in_limit[0],
		context_icn.base + NODE_HIBW1_DDR_IN_0_LIMIT);
	writel(context_icn.hibw1_ddr_in_limit[1],
		context_icn.base + NODE_HIBW1_DDR_IN_1_LIMIT);
	writel(context_icn.hibw1_ddr_in_limit[2],
		context_icn.base + NODE_HIBW1_DDR_IN_2_LIMIT);

	writel(context_icn.hibw1_ddr_out_prio,
		context_icn.base + NODE_HIBW1_DDR_OUT_0_PRIORITY);

	writel(context_icn.hibw2_esram_in_pri[0],
		context_icn.base + NODE_HIBW2_ESRAM_IN_0_PRIORITY);
	writel(context_icn.hibw2_esram_in_pri[1],
		context_icn.base + NODE_HIBW2_ESRAM_IN_1_PRIORITY);

	writel(context_icn.hibw2_esram_in0_arblimit[0],
		context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_1_LIMIT);
	writel(context_icn.hibw2_esram_in0_arblimit[1],
		context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_2_LIMIT);
	writel(context_icn.hibw2_esram_in0_arblimit[2],
		context_icn.base + NODE_HIBW2_ESRAM_IN_0_ARB_3_LIMIT);

	writel(context_icn.hibw2_esram_in1_arblimit[0],
		context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_1_LIMIT);
	writel(context_icn.hibw2_esram_in1_arblimit[1],
		context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_2_LIMIT);
	writel(context_icn.hibw2_esram_in1_arblimit[2],
		context_icn.base + NODE_HIBW2_ESRAM_IN_1_ARB_3_LIMIT);

	writel(context_icn.hibw2_ddr_in_prio[0],
		context_icn.base + NODE_HIBW2_DDR_IN_0_PRIORITY);
	writel(context_icn.hibw2_ddr_in_prio[1],
		context_icn.base + NODE_HIBW2_DDR_IN_1_PRIORITY);
	writel(context_icn.hibw2_ddr_in_prio[2],
		context_icn.base + NODE_HIBW2_DDR_IN_2_PRIORITY);
	if (cpu_is_u8500v1()) {
		writel(context_icn.hibw2_ddr_in_prio[3],
		       context_icn.base + NODE_HIBW2_DDR_IN_3_PRIORITY);
		writel(context_icn.hibw2_ddr_in_limit[0],
		       context_icn.base + NODE_HIBW2_DDR_IN_0_LIMIT_V1);
		writel(context_icn.hibw2_ddr_in_limit[1],
		       context_icn.base + NODE_HIBW2_DDR_IN_1_LIMIT_V1);
	}
	if (cpu_is_u8500v2()) {
		writel(context_icn.hibw2_ddr_in_limit[0],
		       context_icn.base + NODE_HIBW2_DDR_IN_0_LIMIT);
		writel(context_icn.hibw2_ddr_in_limit[1],
		       context_icn.base + NODE_HIBW2_DDR_IN_1_LIMIT);
		writel(context_icn.hibw2_ddr_in_limit[2],
		       context_icn.base + NODE_HIBW2_DDR_IN_2_LIMIT);
		writel(context_icn.hibw2_ddr_out_prio,
		       context_icn.base + NODE_HIBW2_DDR_OUT_0_PRIORITY);

		writel(context_icn.esram0_in_prio[0],
			context_icn.base + NODE_ESRAM0_IN_0_PRIORITY);
		writel(context_icn.esram0_in_prio[1],
			context_icn.base + NODE_ESRAM0_IN_1_PRIORITY);
		writel(context_icn.esram0_in_prio[2],
			context_icn.base + NODE_ESRAM0_IN_2_PRIORITY);
		writel(context_icn.esram0_in_prio[3],
			context_icn.base + NODE_ESRAM0_IN_3_PRIORITY);

		writel(context_icn.esram0_in_lim[0],
			context_icn.base + NODE_ESRAM0_IN_0_LIMIT);
		writel(context_icn.esram0_in_lim[1],
			context_icn.base + NODE_ESRAM0_IN_1_LIMIT);
		writel(context_icn.esram0_in_lim[2],
			context_icn.base + NODE_ESRAM0_IN_2_LIMIT);
		writel(context_icn.esram0_in_lim[3],
			context_icn.base + NODE_ESRAM0_IN_3_LIMIT);
	}

	writel(context_icn.esram12_in_prio[0],
		context_icn.base + NODE_ESRAM1_2_IN_0_PRIORITY);
	writel(context_icn.esram12_in_prio[1],
		context_icn.base + NODE_ESRAM1_2_IN_1_PRIORITY);
	writel(context_icn.esram12_in_prio[2],
		context_icn.base + NODE_ESRAM1_2_IN_2_PRIORITY);
	writel(context_icn.esram12_in_prio[3],
		context_icn.base + NODE_ESRAM1_2_IN_3_PRIORITY);

	writel(context_icn.esram12_in_arb_lim[0],
		context_icn.base + NODE_ESRAM1_2_IN_0_ARB_1_LIMIT);
	writel(context_icn.esram12_in_arb_lim[1],
		context_icn.base + NODE_ESRAM1_2_IN_0_ARB_2_LIMIT);
	writel(context_icn.esram12_in_arb_lim[2],
		context_icn.base + NODE_ESRAM1_2_IN_1_ARB_1_LIMIT);
	writel(context_icn.esram12_in_arb_lim[3],
		context_icn.base + NODE_ESRAM1_2_IN_1_ARB_2_LIMIT);
	writel(context_icn.esram12_in_arb_lim[4],
		context_icn.base + NODE_ESRAM1_2_IN_2_ARB_1_LIMIT);
	writel(context_icn.esram12_in_arb_lim[5],
		context_icn.base + NODE_ESRAM1_2_IN_2_ARB_2_LIMIT);
	writel(context_icn.esram12_in_arb_lim[6],
		context_icn.base + NODE_ESRAM1_2_IN_3_ARB_1_LIMIT);
	writel(context_icn.esram12_in_arb_lim[7],
		context_icn.base + NODE_ESRAM1_2_IN_3_ARB_2_LIMIT);

	writel(context_icn.esram34_in_prio[0],
		context_icn.base + NODE_ESRAM3_4_IN_0_PRIORITY);
	writel(context_icn.esram34_in_prio[1],
		context_icn.base + NODE_ESRAM3_4_IN_1_PRIORITY);
	writel(context_icn.esram34_in_prio[2],
		context_icn.base + NODE_ESRAM3_4_IN_2_PRIORITY);
	writel(context_icn.esram34_in_prio[3],
		context_icn.base + NODE_ESRAM3_4_IN_3_PRIORITY);

	writel(context_icn.esram34_in_arb_lim[0],
		context_icn.base + NODE_ESRAM3_4_IN_0_ARB_1_LIMIT);
	writel(context_icn.esram34_in_arb_lim[1],
		context_icn.base + NODE_ESRAM3_4_IN_0_ARB_2_LIMIT);
	writel(context_icn.esram34_in_arb_lim[2],
		context_icn.base + NODE_ESRAM3_4_IN_1_ARB_1_LIMIT);
	writel(context_icn.esram34_in_arb_lim[3],
		context_icn.base + NODE_ESRAM3_4_IN_1_ARB_2_LIMIT);
	writel(context_icn.esram34_in_arb_lim[4],
		context_icn.base + NODE_ESRAM3_4_IN_2_ARB_1_LIMIT);
	writel(context_icn.esram34_in_arb_lim[5],
		context_icn.base + NODE_ESRAM3_4_IN_2_ARB_2_LIMIT);
	writel(context_icn.esram34_in_arb_lim[6],
		context_icn.base + NODE_ESRAM3_4_IN_3_ARB_1_LIMIT);
	writel(context_icn.esram34_in_arb_lim[7],
		context_icn.base + NODE_ESRAM3_4_IN_3_ARB_2_LIMIT);

}

void u8500_context_init(void)
{
	context_icn.base = ioremap(U8500_ICN_BASE, SZ_8K);
}
