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

/* Two alkaline AAA cells at the end of a TX burst, 0.5 % units. A rough
 * piecewise fit of the AAA discharge curve under a light load, 1.0 V per
 * cell as empty; approximate. */
static uint8_t percent(uint16_t mv)
{
    uint16_t p;
    if (mv >= 2900u) p = 100;
    else if (mv > 2600u) p = 75u + (mv-2600u)*25u/300u;
    else if (mv > 2400u) p = 50u + (mv-2400u)*25u/200u;
    else if (mv > 2200u) p = 20u + (mv-2200u)*30u/200u;
    else if (mv > 2000u) p = (mv-2000u)*20u/200u;
    else p = 0;
    return (uint8_t)(p*2u);
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
        return;
    }
    mv = BGT1_MILLIVOLTS(raw);
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
