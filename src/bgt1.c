/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
/* EfektaLab BGT1 board: battery only here, the DS18B20 temperature comes
 * from sensors.c. */
#include "bgt1.h"
#include "sensors.h"

/* SWRU191F 7.18 P2INP.PDUP0: port 0 pull-down. */
#define PDUP0 0x20u

void bgt1_startup(void)
{
    MMIO_WRITE(SOC_IEN0, 0);
    MMIO_WRITE(SOC_IEN1, 0);
    MMIO_WRITE(SOC_IEN2, 0);
    /* Latches first, then direction: LED off, DS18B20 unpowered, HGM on,
     * PA_EN and LNA_EN low (front end off until radio_setup). The 1-Wire
     * line and the unused port 0 inputs are pulled down. */
    MMIO_CLEAR(SOC_P0, BOARD_LED | BOARD_ONEWIRE_POWER | BOARD_ONEWIRE);
    MMIO_SET(SOC_P0, BOARD_HGM);
    MMIO_CLEAR(SOC_P0SEL, BOARD_LED | BOARD_ONEWIRE_POWER | BOARD_HGM);
    MMIO_SET(SOC_P0DIR, BOARD_LED | BOARD_ONEWIRE_POWER | BOARD_HGM);
    MMIO_SET(SOC_P2INP, PDUP0);
    MMIO_CLEAR(SOC_P1, BOARD_PA_EN | BOARD_LNA_EN);
    MMIO_CLEAR(SOC_P1SEL, BOARD_PA_EN | BOARD_LNA_EN);
    MMIO_SET(SOC_P1DIR, BOARD_PA_EN | BOARD_LNA_EN);
}

void bgt1_power(uint8_t on)
{
    if (on) {
        MMIO_SET(SOC_P0, BOARD_ONEWIRE_POWER);
        sensors_bus(1);
        return;
    }
    sensors_bus(0);
    MMIO_CLEAR(SOC_P0, BOARD_ONEWIRE_POWER);
}

/* n single conversions (ADCCON3, SWRU191F 12.2.10) averaged as 14-bit
 * positive codes; FFFF when a conversion misses its poll bound. */
static uint16_t adc(uint8_t command, uint8_t n)
{
    uint32_t sum = 0;
    uint16_t polls, code;
    uint8_t i, low;
    for (i = n; i; i--) {
        (void)MMIO_READ(SOC_ADCH);
        MMIO_WRITE(SOC_ADCCON3, command);
        for (polls = BGT1_ADC_POLLS; !(MMIO_READ(SOC_ADCCON1) & 0x80u); )
            if (!--polls) return BGT1_INVALID_U16;
        low = MMIO_READ(SOC_ADCL);
        code = (uint16_t)MMIO_READ(SOC_ADCH) << 8 | low;
        if (!(code & 0x8000u)) sum += code >> 2;
    }
    return (uint16_t)(sum / n);
}

/* Two alkaline AAA cells at the end of a TX burst, 0.5 % units: linear
 * between the points of an approximate alkaline discharge curve, about
 * 1.58 V per cell full and 1.0 V (the CC2530's 2.0 V minimum) empty. */
static const MCU_CODE uint16_t curve_mv[] = {3150u, 3000u, 2800u, 2600u, 2400u, 2200u, 2000u};
static const MCU_CODE uint8_t curve_half_percent[] = {200u, 170u, 124u, 80u, 44u, 16u, 0u};

static uint8_t percent(uint16_t mv)
{
    uint8_t i, high, low;
    uint16_t top, bottom;
    if (mv >= curve_mv[0]) return curve_half_percent[0];
    for (i = 1; i < sizeof(curve_mv) / sizeof(curve_mv[0]); i++) {
        bottom = curve_mv[i];
        if (mv > bottom) {
            top = curve_mv[i-1u];
            high = curve_half_percent[i-1u];
            low = curve_half_percent[i];
            return (uint8_t)(low + (uint16_t)(mv - bottom) * (uint8_t)(high - low) / (uint16_t)(top - bottom));
        }
    }
    return 0;
}

uint16_t bgt1_vdd(void)
{
    return adc(0x3fu, 1);
}

void bgt1_battery(gpd_values_t MCU_XDATA *v, uint16_t raw)
{
    uint16_t mv;
    if (raw > BGT1_ADC_MAX) {
        v->voltage = v->percent = 0xffu;
        v->millivolts = (int16_t)BGT1_INVALID_S16;
        return;
    }
    mv = BGT1_MILLIVOLTS(raw);
    v->millivolts = (int16_t)mv;
    v->voltage = (uint8_t)((mv+50u)/100u);
    v->percent = percent(mv);
}

/* Battery (VDD/3, internal reference); the temperature is invalid until
 * a valid DS18B20 reading replaces it. */
void bgt1_measure(gpd_values_t MCU_XDATA *v)
{
    bgt1_battery(v, adc(0x3fu, 5));
    v->temperature = (int16_t)BGT1_INVALID_S16;
}
