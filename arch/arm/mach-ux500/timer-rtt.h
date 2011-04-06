/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com>
 *
 */
#ifndef TIMER_RTT_H
#define TIMER_RTT_H

/**
 * u8500_rtc_adjust_next_wakeup()
 *
 * @delta_in_us: delta time to wake up. Can be negative, to wake up closer
 * in time.
 *
 * This function is needed due to there is no other timer available and in
 * some sleep cases the PRCMU need to wake up earlier than the scheduler
 * wants just to have enough time to start one or more PLLs before the
 * ARM can start executing.
 * Returns -EINVAL if the wake up time can't be adjusted.
 */
int u8500_rtc_adjust_next_wakeup(int delta_in_us);

#endif
