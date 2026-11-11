/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
#include "sensors.h"
#include "onewire.h"

#define T1_DIV8_FREE 0x05u
#define OW_SKIP_ROM 0xccu
#define OW_CONVERT 0x44u
#define OW_READ 0xbeu
#define OW_WRITE 0x4eu
/* Volatile alarm bytes: a DS18B20 that lost power returns its EEPROM
 * values instead, so stale (85 C power-on) data is rejected. */
#define DS_TH 0x00u
#define DS_TL 0x64u
/* SWRU191F 7.18 P2INP.PDUP0: port 0 pull-down. */
#define PDUP0 0x20u
#define OW_LINE() (MMIO_READ(SOC_P0) & BOARD_ONEWIRE)

static MCU_XDATA uint8_t buf[9];

static uint16_t ticks(void)
{
    uint8_t low = MMIO_READ(SOC_T1CNTL);
    return (uint16_t)MMIO_READ(SOC_T1CNTH) << 8 | low;
}

/* Busy until us microseconds after start (Timer 1 at 2 MHz). */
static void until(uint16_t start, uint16_t us)
{
    while ((uint16_t)(ticks() - start) < (uint16_t)(us << 1)) {}
}

static void wait_us(uint16_t us)
{
    until(ticks(), us);
}

/* The 1-Wire line on the port 0 pulls: up while measuring, down otherwise
 * (the vendor firmware's sleep state of port 0). */
void sensors_bus(uint8_t on)
{
    MMIO_CLEAR(SOC_P0DIR, BOARD_ONEWIRE);
    if (on) {
        MMIO_CLEAR(SOC_P2INP, PDUP0);
        MMIO_WRITE(SOC_T1CTL, T1_DIV8_FREE);
    } else {
        MMIO_WRITE(SOC_T1CTL, 0);
        MMIO_SET(SOC_P2INP, PDUP0);
    }
}

/* 1-Wire at standard speed; each slot is timed from its falling edge. */
static void ow_low(void)
{
    MMIO_CLEAR(SOC_P0, BOARD_ONEWIRE);
    MMIO_SET(SOC_P0DIR, BOARD_ONEWIRE);
}

static void ow_release(void)
{
    MMIO_CLEAR(SOC_P0DIR, BOARD_ONEWIRE);
}

/* Reset; 1 when a device answered with a presence pulse. */
static uint8_t ow_reset(void)
{
    uint16_t start;
    uint8_t present;
    ow_low();
    wait_us(480);
    start = ticks();
    ow_release();
    until(start, 70);
    present = !OW_LINE();
    until(start, 480);
    return present && OW_LINE();
}

static uint8_t ow_bit(uint8_t b)
{
    uint16_t start = ticks();
    ow_low();
    until(start, b ? 2u : 62u);
    ow_release();
    if (b) {
        until(start, 12);
        b = OW_LINE() ? 1u : 0u;
    }
    until(start, 70);
    return b;
}

static uint8_t ow_byte(uint8_t b)
{
    uint8_t i, r = 0;
    for (i = 0; i < 8u; i++) {
        r = (uint8_t)(r >> 1 | (ow_bit(b & 1u) ? 0x80u : 0u));
        b >>= 1;
    }
    return r;
}

uint8_t sensors_detect(void)
{
    uint8_t found = 0;
    if (ow_reset()) found |= GPD_HAS_DS18B20;
    ow_release();
    return found;
}

uint8_t ds18b20_start(void)
{
    if (!ow_reset()) return 0;
    ow_byte(OW_SKIP_ROM); ow_byte(OW_WRITE);
    ow_byte(DS_TH); ow_byte(DS_TL); ow_byte(DS18B20_CONFIG);
    if (!ow_reset()) return 0;
    ow_byte(OW_SKIP_ROM); ow_byte(OW_CONVERT);
    return ds18b20_busy();
}

/* An externally powered DS18B20 reads 0 while converting. */
uint8_t ds18b20_busy(void)
{
    uint8_t busy = !ow_bit(1);
    ow_release();
    return busy;
}

void ds18b20_read(gpd_values_t MCU_XDATA *v)
{
    uint8_t i;
    int16_t raw;
    v->temperature = (int16_t)BGT1_INVALID_S16;
    if (!ow_reset()) return;
    ow_byte(OW_SKIP_ROM); ow_byte(OW_READ);
    for (i = 0; i < 9u; i++) buf[i] = ow_byte(0xffu);
    ow_release();
    if (onewire_crc8(buf, 8) != buf[8] || buf[2] != DS_TH || buf[3] != DS_TL ||
        buf[4] != DS18B20_CONFIG)
        return;
    raw = (int16_t)((uint16_t)buf[1] << 8 | (buf[0] & DS18B20_MASK));
    if (raw < -55*16 || raw > 125*16) return;
    v->temperature = (int16_t)(((int32_t)raw * 25) / 4);
}
