/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (C) 2016 Saleem Rashid <trezor@saleemrashid.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>
#include "supervise.h"

extern volatile uint32_t system_millis_poweroff_start;
extern uint8_t ucTimeFlag;
extern uint8_t g_ucLanguageFlag;

#define autoPowerOffMsDefault (90 * 1000U)

#define POWER_OFF_TIMER_ENBALE() (ucTimeFlag |= 0x01)
#define POWER_OFF_TIMER_CLEAR() (ucTimeFlag &= 0xFE)
#define POWER_OFF_TIMER_READY() (ucTimeFlag & 0x01)

void delay_time(uint32_t uiDelay_Ms);
void delay_us(uint32_t uiDelay_us);

void timer_init(void);

#if EMULATOR
uint32_t timer_ms(void);
#else
#define timer_ms svc_timer_ms
#endif

#endif
