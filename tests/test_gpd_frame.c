/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026, cc2530-zigbee contributors. See LICENSE.
 */
/* The Green Power frames of the BGT1 GP firmware (include/gpd_frame.h)
 * against hand-assembled bytes (Zigbee Green Power Basic: MAC header, NWK
 * frame control 0x0C, SrcID, command, payload). With an argument, prints the
 * frames as hex for an external parser cross-check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MCU_XDATA
#include "gpd_frame.h"
#include "onewire.h"
#if GPD_SECURITY
#include "aes_reference.h"

void gpd_aes(const uint8_t *key, uint8_t *block)
{
    aes_reference_encrypt(key, block, block);
}

/* zigpy.zgp.crypto (AESCCM of the Python cryptography package) for SrcID
 * 0x0EF62815 and the key C0..CF: the key protected with the TC-LK and its
 * MIC; the report of main() with security frame counter 0x401 and the
 * Decommissioning one with 0x12345678, SecurityLevel 3. */
static const uint8_t gpd_key[16] = {
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf
};
static const uint8_t protected_key[20] = {
    0x47, 0x4e, 0x69, 0xa9, 0xd4, 0xde, 0x13, 0x69, 0xfc, 0x7f, 0xcd, 0x93, 0xb0, 0xa1, 0xd9, 0xfa,
    0xf7, 0x12, 0xd5, 0x07
};
static const uint8_t secured_report[] = {
    0x01, 0x08, 0x43, 0xff, 0xff, 0xff, 0xff, 0x8c, 0x38, 0x15, 0x28, 0xf6, 0x0e, 0x01, 0x04, 0x00, 0x00,
    0xcd, 0x48, 0xa1, 0x02, 0xe5, 0x0e, 0x9d, 0x0c, 0x28, 0xac, 0x61, 0xbe, 0x9c, 0xf7, 0x32, 0x21,
    0x7f, 0x12, 0x2e, 0xdc, 0x30, 0x3b, 0x53, 0x79, 0xe5, 0x89, 0xf2, 0xaa, 0x53, 0x2a, 0xff
};
static const uint8_t secured_decommissioning[] = {
    0x01, 0x08, 0x47, 0xff, 0xff, 0xff, 0xff, 0x8c, 0x38, 0x15, 0x28, 0xf6, 0x0e, 0x78, 0x56, 0x34, 0x12,
    0x9a, 0x7e, 0xc8, 0xf1, 0x57
};
#define COMMISSIONING(f, seq, src, sensors) gpd_commissioning(f, seq, src, sensors, gpd_key, 0x400u)
#else
#define COMMISSIONING(f, seq, src, sensors) gpd_commissioning(f, seq, src, sensors)
#endif

#define CHECK(x) do { checks++; if (!(x)) { \
    fprintf(stderr, "gpd_frame: line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static unsigned long checks;
static uint8_t expected[GPD_COMMISSIONING_LENGTH];

/* A NoSecurity Commissioning GPDF of n bytes as built with GPD_SECURITY:
 * options C5, extended options F3, the protected key and its MIC, the
 * outgoing counter 0x400, then the application information onwards. */
static const uint8_t *commissioned(const uint8_t *plain, uint8_t n)
{
#if GPD_SECURITY
    static const uint8_t counter[4] = {0x00, 0x04, 0x00, 0x00};
    memcpy(expected, plain, 14);
    expected[14] = 0xc5; expected[15] = 0xf3;
    memcpy(expected + 16, protected_key, 20);
    memcpy(expected + 36, counter, 4);
    memcpy(expected + 40, plain + 15, n - 15u);
#else
    memcpy(expected, plain, n);
#endif
    return expected;
}
#define COMMISSIONED_LENGTH(n) ((uint8_t)((n) + GPD_COMMISSIONING_LENGTH - (GPD_HEADER + 1u + 14u)))

static void hex(const char *name, const uint8_t *f, uint8_t n)
{
    uint8_t i;
    printf("%s ", name);
    for (i = 0; i < n; i++) printf("%02x", f[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    static const uint8_t ieee[8] = {0x15, 0x28, 0xf6, 0x0e, 0x00, 0x4b, 0x12, 0x00};
    /* TemperatureSensor 0x30, model F202 (DS18B20), Power Configuration,
     * Temperature Measurement and Electrical Measurement; temperature,
     * battery and battery millivolt records. */
    static const uint8_t commissioning[] = {
        0x01, 0x08, 0x42, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x15, 0x28, 0xf6, 0x0e, 0xe0,
        0x30, 0x45, 0x0b, 0x6b, 0x6d, 0x02, 0xf2, 0x03,
        0x01, 0x00, 0x02, 0x04, 0x04, 0x0b
    };
    static const uint8_t report[] = {
        0x01, 0x08, 0x43, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x15, 0x28, 0xf6, 0x0e, 0xa2,
        0x02, 0x04, 0x00, 0x00, 0x29, 0x0c, 0xfe,
        0x01, 0x00, 0x20, 0x00, 0x20, 0x1e,
        0x01, 0x00, 0x21, 0x00, 0x20, 0xc8,
        0x04, 0x0b, 0x00, 0x01, 0x29, 0xc9, 0x0b
    };
    /* Maintenance Channel Requests announcing channel 15 (0b0100 twice). */
    static const uint8_t request_more[] = {0x01, 0x08, 0x45, 0xff, 0xff, 0xff, 0xff, 0x4d, 0xe3, 0x44};
    static const uint8_t request_last[] = {0x01, 0x08, 0x46, 0xff, 0xff, 0xff, 0xff, 0x0d, 0xe3, 0xff};
    static const uint8_t decommissioning[] = {
        0x01, 0x08, 0x47, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x15, 0x28, 0xf6, 0x0e, 0xe1
    };
    /* Channel Configurations (two FCS status bytes): from a proxy with a
     * short source address and PAN ID compression, channel 20 with the
     * Basic bit; without a source address, channel 11. */
    static const uint8_t config_proxy[] = {
        0x41, 0x88, 0x10, 0x62, 0x1a, 0xff, 0xff, 0x34, 0x12, 0x0d, 0xf3, 0x19, 0xd0, 0x80
    };
    static const uint8_t config_bare[] = {0x01, 0x08, 0x11, 0xff, 0xff, 0xff, 0xff, 0x0d, 0xf3, 0x00, 0x00, 0x80};
    /* Maxim AN27: ROM 02 1C B8 01 00 00 00, CRC A2. */
    static const uint8_t rom[8] = {0x02, 0x1c, 0xb8, 0x01, 0x00, 0x00, 0x00, 0xa2};
    uint8_t src[4], f[GPD_FRAME_LENGTH + 1u], m[sizeof(config_proxy)], n, z[4] = {0}, high[4] = {0xfe, 0xff, 0xff, 0xff};
    gpd_values_t v = { -500, 30, 200, 3017 };
    (void)argv;

    gpd_source(src, ieee);
    CHECK(src[0] == 0x15 && src[1] == 0x28 && src[2] == 0xf6 && src[3] == 0x0e);
    /* Reserved SrcIDs: 0 and FFFFFFF9..FFFFFFFF. */
    gpd_source(src, z);
    CHECK(src[0] == 1 && !src[1] && !src[2] && !src[3]);
    gpd_source(src, high);
    CHECK(src[0] == 0xf6);
    high[0] = 0xff; gpd_source(src, high); CHECK(src[0] == 0xf7);
    high[0] = 0xf9; gpd_source(src, high); CHECK(src[0] == 0xf1);
    high[0] = 0xf8; gpd_source(src, high); CHECK(src[0] == 0xf8);
    high[3] = 0xfe; high[0] = 0xff; gpd_source(src, high); CHECK(src[0] == 0xff);

    gpd_source(src, ieee);
    memset(f, 0xaa, sizeof(f));
    n = COMMISSIONING(f, 0x42, src, GPD_HAS_DS18B20);
    CHECK(n == GPD_COMMISSIONING_LENGTH && n == COMMISSIONED_LENGTH(sizeof(commissioning)));
    CHECK(!memcmp(f, commissioned(commissioning, sizeof(commissioning)), n) && f[n] == 0xaa);
    if (argc > 1) hex("commissioning", f, n);
    memset(f, 0xaa, sizeof(f));
    n = gpd_report(f, 0x43, src, &v);
    CHECK(n == GPD_REPORT_LENGTH && n == sizeof(report));
    CHECK(!memcmp(f, report, n) && f[n] == 0xaa);
    if (argc > 1) hex("report", f, n);
    /* Without the DS18B20: model F200, the same clusters. */
    n = COMMISSIONING(f, 0x42, src, 0);
    commissioned(commissioning, sizeof(commissioning));
    CHECK(n == COMMISSIONED_LENGTH(sizeof(commissioning)) && f[n-9u] == 0x00);
    CHECK(!memcmp(f, expected, n - 9u) && !memcmp(f + n - 8u, expected + n - 8u, 8));
    /* PSDU with the FCS within aMaxPHYPacketSize. */
    CHECK(GPD_FRAME_LENGTH + 2u <= 127u);
    CHECK(GPD_COMMISSIONING_LENGTH <= GPD_FRAME_LENGTH && GPD_REPORT_LENGTH + GPD_SECURITY_OVERHEAD <= GPD_FRAME_LENGTH);
#if GPD_SECURITY
    /* The report and the Decommissioning GPDF secured. */
    memset(f, 0xaa, sizeof(f));
    n = gpd_secure(f, gpd_report(f, 0x43, src, &v), gpd_key, 0x401u);
    CHECK(n == sizeof(secured_report) && !memcmp(f, secured_report, n) && f[n] == 0xaa);
    if (argc > 1) hex("secured-report", f, n);
    memset(f, 0xaa, sizeof(f));
    n = gpd_secure(f, gpd_decommissioning(f, 0x47, src), gpd_key, 0x12345678u);
    CHECK(n == GPD_DECOMMISSIONING_SECURED && n == sizeof(secured_decommissioning));
    CHECK(!memcmp(f, secured_decommissioning, n) && f[n] == 0xaa);
    /* A tampered key changes the MIC. */
    memset(f, 0xaa, sizeof(f));
    memcpy(expected, gpd_key, 16); expected[15] ^= 1;
    n = gpd_secure(f, gpd_decommissioning(f, 0x47, src), expected, 0x12345678u);
    CHECK(memcmp(f + n - 4u, secured_decommissioning + n - 4u, 4));
#endif

    CHECK(gpd_channel_request(f, 0x45, 1, 15) == sizeof(request_more));
    CHECK(!memcmp(f, request_more, sizeof(request_more)));
    if (argc > 1) hex("channel-request", f, sizeof(request_more));
    CHECK(gpd_channel_request(f, 0x46, 0, 26) == sizeof(request_last));
    CHECK(!memcmp(f, request_last, sizeof(request_last)));
    CHECK(gpd_decommissioning(f, 0x47, src) == sizeof(decommissioning));
    CHECK(!memcmp(f, decommissioning, sizeof(decommissioning)));

    memcpy(m, config_proxy, sizeof(m));
    CHECK(gpd_channel_configuration(m, sizeof(config_proxy)) == 20u);
    memcpy(m, config_bare, sizeof(config_bare));
    CHECK(gpd_channel_configuration(m, sizeof(config_bare)) == 11u);
    CHECK(!gpd_channel_configuration(m, sizeof(config_bare) - 1u));
    m[7] = GPD_MAINTENANCE_MORE;
    CHECK(!gpd_channel_configuration(m, sizeof(config_bare)));
    m[7] = GPD_MAINTENANCE; m[8] = GPD_CHANNEL_REQUEST;
    CHECK(!gpd_channel_configuration(m, sizeof(config_bare)));
    m[8] = GPD_CHANNEL_CONFIGURATION; m[0] = 0x09;   /* secured */
    CHECK(!gpd_channel_configuration(m, sizeof(config_bare)));
    m[0] = 0x03;                                     /* MAC command */
    CHECK(!gpd_channel_configuration(m, sizeof(config_bare)));
    memcpy(m, config_proxy, sizeof(m));
    m[1] = 0xc8;                                     /* long source: truncated */
    CHECK(!gpd_channel_configuration(m, sizeof(config_proxy)));

    CHECK(onewire_crc8(rom, 7) == 0xa2 && onewire_crc8(rom, 8) == 0);

    /* Change-based reporting: 0.25 C from the last sent, or validity. */
    CHECK(!gpd_temperature_changed(2112, 2100, 25));
    CHECK(gpd_temperature_changed(2125, 2100, 25) && gpd_temperature_changed(2075, 2100, 25));
    CHECK(!gpd_temperature_changed(-12, 12, 25) && gpd_temperature_changed(-13, 12, 25));
    CHECK(gpd_temperature_changed((int16_t)0x8000, 2100, 25) && gpd_temperature_changed(2100, (int16_t)0x8000, 25));
    CHECK(!gpd_temperature_changed((int16_t)0x8000, (int16_t)0x8000, 25));
    CHECK(gpd_temperature_changed(12500, -5500, 25));

    if (argc <= 1) printf("gpd_frame: %lu checks passed\n", checks);
    return 0;
}
