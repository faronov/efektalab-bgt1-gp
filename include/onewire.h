/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
/* Shared by the firmware and the host test. */
#ifndef BGT1_ONEWIRE_H
#define BGT1_ONEWIRE_H

#include <stdint.h>

/* Maxim 1-Wire CRC8 (X^8+X^5+X^4+1, reflected 0x8C). */
__attribute__((unused)) static uint8_t onewire_crc8(const uint8_t *d, uint8_t n)
{
    uint8_t crc = 0, i, b;
    while (n--) {
        b = *d++;
        for (i = 0; i < 8u; i++) {
            crc = ((crc ^ b) & 1u) ? (uint8_t)((crc >> 1) ^ 0x8cu) : (uint8_t)(crc >> 1);
            b >>= 1;
        }
    }
    return crc;
}

#endif
