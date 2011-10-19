/*
 * Copyright (C) 2009 ST-Ericsson SA
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _UX500_STE_AUDIO_H_
#define _UX500_STE_AUDIO_H_


struct ab8500_audio_platform_data {
	int (*ste_gpio_altf_init) (void);
	int (*ste_gpio_altf_exit) (void);
};

#endif /* _UX500_STE_AUDIO_H_ */
