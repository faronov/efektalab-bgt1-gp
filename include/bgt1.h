/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
#ifndef BGT1_H
#define BGT1_H
/* EfektaLab BGT1 board: startup pin state, DS18B20 supply and battery
 * measurement; the DS18B20 temperature comes from sensors.c. */
#include "board.h"
#include "gpd_frame.h"

#define BGT1_ADC_POLLS 4000u
#define BGT1_INVALID_U16 0xffffu
#define BGT1_INVALID_S16 0x8000u
/* CC2530 single-ended 14-bit positive half range. */
#define BGT1_ADC_MAX 8191u
/* mV = raw * 0.443 (VDD/3, internal 1.15 V reference). */
#define BGT1_MILLIVOLTS(raw) ((uint16_t)(((uint32_t)(raw) * 443u) / 1000u))

/* Interrupts off, LED and DS18B20 supply off, front end off: the first
 * thing main does. */
void bgt1_startup(void);
/* DS18B20 supply and 1-Wire pulls on or off. */
void bgt1_power(uint8_t on);
/* Battery into v, the temperature invalid (the supply must have warmed up). */
void bgt1_measure(gpd_values_t MCU_XDATA *v);
/* One raw VDD/3 conversion (internal reference), BGT1_INVALID_U16 on a fault. */
uint16_t bgt1_vdd(void);
/* Battery voltage and percentage of v from a raw VDD/3 average. */
void bgt1_battery(gpd_values_t MCU_XDATA *v, uint16_t raw);

#endif
