/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Rickard Andersson <rickard.andersson@stericsson.com> for
 *         ST-Ericsson.
 * License terms: GNU General Public License (GPL) version 2
 *
 */

#ifndef PM_COMMON_H
#define PM_COMMON_H

#ifdef CONFIG_PM
struct dev_power_domain;
extern struct dev_power_domain ux500_dev_power_domain;
extern struct dev_power_domain ux500_amba_dev_power_domain;
#endif

#endif
