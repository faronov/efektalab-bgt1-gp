# EfektaLab BGT1 Green Power firmware (CC2530)

Firmware for the EfektaLab BGT1 temperature sensor (CC2530 with a PA/LNA
front end, DS18B20, two AAA cells) that works as a **Zigbee Green Power
Device (GPD)**. It never joins a network. Every 60 s it wakes from PM2,
measures, and broadcasts a GPDF when the temperature changed, then goes back
to sleep. A Green Power proxy or sink, such as ZHA with the bundled quirk or
Zigbee2MQTT with the bundled converter, receives it.

Split from the `BOARD=bgt1` build of
[faronov/flower-cc2530-gp](https://github.com/faronov/flower-cc2530-gp).

- Image: about 17 KB, non-banked.
- Toolchain: LLVM MCS-51 ([faronov/llvm-project](https://github.com/faronov/llvm-project)),
  pinned through the [cc2530-zigbee](https://github.com/faronov/cc2530-zigbee)
  submodule. Only the stack's clock, Sleep Timer and AES helpers are linked.
- Measurement: DS18B20 temperature (11 bits, 0.125 °C steps, 375 ms
  conversion) and battery voltage. Without a valid DS18B20 reading the
  temperature is reported invalid; there is no chip-sensor fallback. The
  DS18B20 is detected at every start.
- Battery: read at the end of the report's transmissions (under TX load)
  and reported in the next one; the first report after a start uses the idle
  voltage. 0 % at 2.0 V, 100 % from 2.9 V, a rough piecewise curve for two
  alkaline AAA cells; approximate.

## Hardware

The pin map was read from the original Z-Stack firmware:

| Function | Pin |
|---|---|
| LED (active high) | P0.1 |
| Button (active low, pull-up) | P2.0 |
| DS18B20 supply (high while measuring) | P0.2 |
| DS18B20 1-Wire (port 0 internal pulls) | P0.3 |
| PA_EN (radio observation output 0, inverted pa_pd) | P1.1 |
| LNA_EN (radio observation output 1, inverted lna_pd) | P1.4 |
| HGM (high-gain mode, always high) | P0.7 |

Front end: Z-Stack's CC2590/CC2591 wiring (`paLnaChip` 2). The original
firmware uses Z-Stack's CC2590 TX power table; this build keeps
`GPD_TXPOWER` 0xF5, that table's highest setting (about +11 dBm with a
CC2590, more with a CC2591). Which of the two is fitted is not known.

## Radio protocol

- Frame type: Green Power Device Frames, ApplicationID 0 (SrcID),
  unidirectional, security level 3 (see Security).
- SrcID: the low 32 bits of the chip IEEE address, avoiding the reserved
  values.
- Commissioning frame (0xE0): DeviceID 0x30 (TemperatureSensor),
  manufacturer 0x6D6B, model 0xF202 (`0xF200 | sensors`, bit 1 DS18B20;
  0xF200 when no DS18B20 answers). Server clusters are 0x0001 and 0x0402.
- Reports: a Multi-Cluster Reporting frame (0xA2) with, in their ZCL
  encodings:
  - temperature (0x0402), 0x8000 when invalid;
  - battery voltage (0x0001/0x0020) and battery remaining (0x0001/0x0021).
- Change-based reporting: the sensor wakes every 60 s (`GPD_INTERVAL`) but
  transmits only when the temperature moved by 0.25 °C or more from the last
  report, became valid or invalid, or after 30 minutes without a report
  (heartbeat). The button and (re)commissioning always report.
- Repeats and sleep: each GPDF is sent 3 times, 20 ms apart, with PM2
  between repetitions and between rounds, and while the DS18B20 supply
  settles and the sensor converts.

### Channel search and commissioning

Commissioning runs on the first start, when the detected sensor set
changes, and after a 3-s button press. It does up to 12 rounds, 10 s apart.
Each round works as follows:

1. **Channel search (GP Basic A.3.9.1).** Up to 6 attempts, 400 ms apart.
   Each attempt sends a Maintenance *Channel Request* (0xE3) on every
   channel, with Auto-Commissioning set. The *hint* channel (the saved one,
   or `GPD_CHANNEL` = 15) goes last, with Auto-Commissioning cleared. The
   device then listens there for 18–40 ms.
2. **Answer.** A GP proxy of the network (the TempMaster) can answer with a
   *Channel Configuration* (0xF3) carrying the operational channel. It does
   so only while the sink is in commissioning mode, which in ZHA means
   permit join is open.
3. **Commissioning.** An answer ends the rounds: one Commissioning frame
   and a report go out on that channel, and the channel is saved. Without an
   answer, the round sends them on the hint channel instead.

The ZHA coordinator itself cannot answer: bellows/EZSP never sends GP
frames to a GPD. The answer therefore needs a GP proxy, for example an IKEA
TRÅDFRI router, within radio range. Without one, keep the network on
channel 15 or rebuild with another hint:
`make firmware GPD_DEFINES=-DGPD_CHANNEL=20`.

### Button and LED

| Button (P2.0)  | Action |
|----------------|--------|
| short press    | measure and report now (LED flash) |
| hold 3 s       | LED on; on release, search the channel and commission |
| hold 10 s      | LED blinks; on release, send Decommissioning (0xE1), clear the settings and stop reporting |

The button wakes the device from PM2 through the port 2 interrupt.

LED (P0.1) signals:
- one flash per channel-search attempt;
- 1 s on: the channel was found;
- 3 blinks: the channel was not found (the hint channel is used);
- 5 blinks: decommissioned.

### Security

Reports and Decommissioning are sent at GP security level 3 (encrypted,
4-byte MIC, AES-CCM* on the CC2530 AES coprocessor) with an individual
out-of-the-box key (KeyType 4). Channel Requests are unprotected, as the
specification requires.

- Key: 16 random bytes, made on the first commissioning from the radio's
  random bits (RFRND, receiver on) and whitened with AES. It survives
  recommissioning and goes on Decommissioning (a 10-s press) or a chip erase.
  The sink then sees a new key and the device must be paired again.
- Commissioning frame: the key is sent encrypted with the ZigBee
  Alliance default TC link key (`ZigBeeAlliance09`) with its MIC, plus the
  outgoing frame counter, as Zigbee2MQTT (zigbee-herdsman) and ZHA (zigpy)
  expect.
- Frame counter: 32 bits, never reused for a given key. Flash holds a lease of
  1024 counter values; each reboot starts at the next lease.
- Firmware that starts without a key (for example after an upgrade without a
  chip erase) commissions again.
- `GPD_DEFINES=-DGPD_SECURITY=0` builds the unsecured variant.

### Settings

The channel, the sensor set, the key and the frame-counter lease are kept in
flash page 125 (0x3E800), as a log of 4-byte records written by DMA. A chip
erase (`cc-tool -e`) therefore means a first start and a new key.

## Pairing with ZHA

1. Copy `quirks/zha_quirk_efektalab_bgt1_gp.py` ("EfektaLab BGT1 GP") to
   `<HA config>/custom_zha_quirks/` and restart Home Assistant. This needs a
   ZHA build with Green Power support.
2. Permit joining.
3. Power the sensor (first start), or hold its button for 3 s.

Each accepted Commissioning frame re-adds the device in ZHA while permit
join is open. After pairing, close permit join.

## Zigbee2MQTT

Copy the external converter `z2m/efektalab_bgt1_gp.mjs` to
`<z2m data>/external_converters/` and restart Zigbee2MQTT. zigbee-herdsman
names the device `GreenPower_48` after its DeviceID, so the converter
matches any Green Power TemperatureSensor. Tested against zigbee-herdsman's
parsing and decryption of this firmware's frames, not with a live
Zigbee2MQTT.

## Flashing

Download `bgt1-gp.hex` from the
[latest release](https://github.com/faronov/efektalab-bgt1-gp/releases/latest)
and write it with a CC Debugger, for example:

```sh
cc-tool -e -w bgt1-gp.hex -v r --reset
```

## Building

```sh
git clone --recursive https://github.com/faronov/efektalab-bgt1-gp
cd efektalab-bgt1-gp
make test                       # frame builder, host (ASan/UBSan)
make test-quirk                 # needs: pip install pytest zha zha-quirks
make firmware LLVM_TOOLS=<llvm-build>/bin LLVM_SRC=<llvm-project>
```

`LLVM_SRC` must be the [faronov/llvm-project](https://github.com/faronov/llvm-project)
checkout at the revision in `cc2530-zigbee/tools/llvm/toolchain.json`. The
MCS-51 runtime and startup come from it, and `LLVM_TOOLS` must be built
from the same revision (`clang`, `lld`, `llvm-ar`, `llvm-objcopy`; see
`.github/workflows/ci.yml`). The output is
`build/firmware/bgt1-gp.{hex,elf,map}`.

The image is linked with `cc2530-root.ld`. It is LLVM's `cc2530.ld` without
the code bank sections, so every function stays in the 32-KB common area.

The image has no DATA/IDATA globals, so the build patches the startup to
`mov sp, #0x47`. The hardware stack then gets IDATA 0x48–0xFF (184 bytes)
instead of 0x80–0xFF; the deepest path needs about 157 bytes. After
linking, `tools/stack.py` estimates the worst case from the disassembly and
fails the build if it does not fit.

## Releases

Pushing a `v*` tag builds the firmware in CI. The workflow then publishes
`bgt1-gp.hex`, `bgt1-gp.elf`, `SHA256SUMS`, the ZHA quirk and the
Zigbee2MQTT converter as a GitHub release.

## License

BSD-3-Clause, see [LICENSE](LICENSE). `cc2530-root.ld` is derived from LLVM
(Apache-2.0 WITH LLVM-exception).
