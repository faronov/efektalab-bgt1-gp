"""Test the bgt1-gp ZHA quirk against frames produced by include/gpd_frame.h."""

from unittest.mock import MagicMock

import pytest

from zha.const import STATE_CHANGED
from zha.quirks import DEVICE_REGISTRY
from zigpy.device import GreenPowerCommandReceived, GreenPowerDevice
from zigpy.zgp.commands import GPCommissioningPayload, GPMultiClusterReportingPayload
from zigpy.zgp.types import ApplicationID, GPDCommandID, SrcID

import zha_quirk_efektalab_bgt1_gp  # noqa: F401  registers the quirk

# tests/test_gpd_frame.c (model 0xF202), after byte 13:
# extended options, the protected key and its MIC, outgoing counter 0x400.
COMMISSIONING = bytes.fromhex(
    "30c5f3474e69a9d4de1369fc7fcd93b0a1d9faf712d507000400000b6b6d02f20301000204040b"
)
# Temperature -5 °C, battery 3.0 V and 100 %, then DCVoltage 3017 mV (newer builds).
REPORT_100MV = bytes.fromhex("02040000290cfe01002000201e0100210020c8")
REPORT = REPORT_100MV + bytes.fromhex("040b000129c90b")


def report_event(payload: bytes) -> GreenPowerCommandReceived:
    report, rest = GPMultiClusterReportingPayload.deserialize(payload)
    assert rest == b""
    return GreenPowerCommandReceived(
        device_ieee="", endpoint_id=0,
        command_id=GPDCommandID.MultiClusterReporting, command=report,
    )


@pytest.mark.parametrize("model", [0xF200, 0xF202])
def test_bgt1_gp_quirk(model):
    frame = bytearray(COMMISSIONING)
    frame[30:32] = model.to_bytes(2, "little")
    commissioning, rest = GPCommissioningPayload.deserialize(bytes(frame))
    assert rest == b""
    assert commissioning.extended_options.key_encrypted
    gpd = GreenPowerDevice(
        MagicMock(), application_id=ApplicationID.SrcID, src_id=SrcID(0x0EF62815)
    )
    gpd.device_id = commissioning.device_id
    gpd.gpd_manufacturer_id = commissioning.manufacturer_id
    gpd.gpd_model_id = commissioning.model_id
    entry = DEVICE_REGISTRY.match_green_power_entry(gpd)
    assert entry is not None
    zha_device = entry.zha_device_factory(DEVICE_REGISTRY.resolve(gpd), MagicMock())
    assert zha_device.name == "EfektaLab BGT1 GP"
    entities = {e.unique_id.rsplit("-", 1)[1]: e for e in zha_device.discover_entities()}
    assert set(entities) == {"temperature", "battery", "battery_voltage"}
    for entity in entities.values():
        entity.on_add()
        assert entity.native_value is None

    gpd.emit(GreenPowerCommandReceived.event_type, report_event(REPORT_100MV))
    values = {name: e.native_value for name, e in entities.items()}
    assert values == {"temperature": -5.0, "battery_voltage": 3.0, "battery": 100.0}

    # Every report emits a state event, also with unchanged values.
    events = {name: [] for name in entities}
    for name, entity in entities.items():
        entity.on_event(STATE_CHANGED, events[name].append)
    for payload in (REPORT, REPORT):
        gpd.emit(GreenPowerCommandReceived.event_type, report_event(payload))
    values = {name: e.native_value for name, e in entities.items()}
    assert values == {"temperature": -5.0, "battery_voltage": 3.017, "battery": 100.0}
    assert {name: len(e) for name, e in events.items()} == {
        "temperature": 2, "battery_voltage": 2, "battery": 2
    }
