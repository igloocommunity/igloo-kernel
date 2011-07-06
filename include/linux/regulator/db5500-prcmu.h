/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com> for ST-Ericsson
 *
 * Interface to power domain regulators on DB5500
 */

#ifndef __DB5500_REGULATOR_H__
#define __DB5500_REGULATOR_H__

#include <linux/regulator/dbx500-prcmu.h>

/* Number of DB5500 regulators and regulator enumeration */
enum db5500_regulator_id {
	U5500_REGULATOR_VAPE,
	U5500_REGULATOR_SWITCH_SGA,
	U5500_REGULATOR_SWITCH_HVA,
	U5500_REGULATOR_SWITCH_SIA,
	U5500_REGULATOR_SWITCH_DISP,
	U5500_REGULATOR_SWITCH_ESRAM12,
	U5500_NUM_REGULATORS
};

#endif
