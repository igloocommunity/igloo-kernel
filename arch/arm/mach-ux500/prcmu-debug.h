/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * License Terms: GNU General Public License v2
 *
 * Author: Martin Persson <martin.persson@stericsson.com> for ST-Ericsson
 *         Etienne Carriere <etienne.carriere@stericsson.com> for ST-Ericsson
 *
 */

#ifndef PRCMU_DEBUG_H
#define PRCMU_DEBUG_H

#ifdef CONFIG_UX500_PRCMU_DEBUG
void prcmu_debug_ape_opp_log(u8 opp);
void prcmu_debug_ddr_opp_log(u8 opp);
int prcmu_debug_init(void);
#else
static inline void prcmu_debug_ape_opp_log(u8 opp) {}
static inline void prcmu_debug_ddr_opp_log(u8 opp) {}
static inline int prcmu_debug_init(void) {}
#endif
#endif
