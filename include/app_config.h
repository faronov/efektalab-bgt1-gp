/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
#ifndef BGT1_APP_CONFIG_H
#define BGT1_APP_CONFIG_H
/* Included by the stack's cc2530_mmio.h for CC2530_APP (board
 * BOARD_EXTERNAL): macros only, no includes. */

/* board.h requires an identity; the GPD itself sends GPD_MANUFACTURER and
 * GPD_MODEL (gpd_frame.h). */
#define BOARD_MANUFACTURER "EfektaLab"
#define BOARD_MODEL "EfektaLab_BGT1_GP"
#define BOARD_MODEL_LENGTH 17u

/* EfektaLab BGT1 (BEE_DEN_T1), pin map read from its Z-Stack firmware:
 * LED P0.1 active high, button P2.0 active low (pull-up), DS18B20 supply
 * P0.2 (high while measuring) and 1-Wire P0.3 on the port 0 pulls (up while
 * measuring, down otherwise, P2INP.PDUP0). PA/LNA front end in the
 * CC2590/CC2591 wiring (Z-Stack paLnaChip 2): PA_EN P1.1 and LNA_EN P1.4 from
 * the radio observation outputs, HGM P0.7 high. Two AAA cells. */
#define BOARD_LED 0x02u
#define BOARD_BUTTON 0x01u
#define BOARD_ONEWIRE_POWER 0x04u
#define BOARD_ONEWIRE 0x08u
#define BOARD_HGM 0x80u
#define BOARD_PA_EN 0x02u
#define BOARD_LNA_EN 0x10u
/* A scheduled report only when the temperature moved by BOARD_REPORT_DELTA
 * (0.01 C: two 11-bit steps) from the last one sent, or its validity
 * changed, or after BOARD_HEARTBEAT intervals without one (30 min). */
#define BOARD_REPORT_DELTA 25
#define BOARD_HEARTBEAT 30u

#define CC2530_APP_REGISTER_LIST(X) \
    X(SOC_T1CNTL, 0xe2) \
    X(SOC_T1CNTH, 0xe3) \
    X(SOC_T1CTL, 0xe4) \
    X(SOC_P2IFG, 0x8b) \
    X(SOC_P2IEN, 0xac) \
    X(SOC_PICTL, 0x8c) \
    X(SOC_IRCON2, 0xe8)

#endif
