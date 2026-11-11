/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
#ifndef BGT1_SENSORS_H
#define BGT1_SENSORS_H
/* DS18B20 on 1-Wire (P0.3, supplied from P0.2). Timer 1 counts 2 MHz
 * (16-MHz tick / 8) for the bus timing while a sensor call runs;
 * interrupts are off. */
#include "bgt1.h"

/* DS18B20 at 11-bit resolution: configuration register, undefined low bit
 * of the result, and conversion time (datasheet tCONV 375 ms). */
#define DS18B20_CONFIG 0x5fu
#define DS18B20_MASK 0xfeu
#define DS18B20_CONVERSION 13107UL  /* Sleep Timer ticks, 400 ms */
/* Clones may convert slower than tCONV at any resolution (a Mysentech-type
 * chip ~500 ms, reading 25 C until done): polled every 50 ms up to 1 s. */
#define DS18B20_POLL 1638UL         /* Sleep Timer ticks, 50 ms */
#define DS18B20_TIMEOUT 32768UL     /* Sleep Timer ticks, 1 s */

/* Supply on and warmed up: GPD_HAS_DS18B20 when a DS18B20 answers. */
uint8_t sensors_detect(void);
/* Configures the resolution and starts a DS18B20 conversion; 0 on error. */
uint8_t ds18b20_start(void);
/* 1 while the started conversion runs. */
uint8_t ds18b20_busy(void);
/* Reads the conversion into v->temperature (invalid on error). */
void ds18b20_read(gpd_values_t MCU_XDATA *v);
/* 1-Wire line pulled up and Timer 1 running (on), or pulled down and
 * Timer 1 stopped (off). */
void sensors_bus(uint8_t on);

#endif
