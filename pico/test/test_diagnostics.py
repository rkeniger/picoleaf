"""Diagnostics JSON and sensor discovery (src/diagnostics.c) through ctypes."""

from __future__ import annotations

import ctypes
import json

import pytest


class Diagnostics(ctypes.Structure):
    _fields_ = [
        ("uptime_us", ctypes.c_uint64),
        ("longest_poll_gap_us", ctypes.c_uint64),
        ("longest_loop_us", ctypes.c_uint64),
        ("sessions_lost", ctypes.c_uint32),
        ("mqtt_reconnects", ctypes.c_uint32),
        ("squares", ctypes.c_uint32),
        ("odd_polls", ctypes.c_uint32),
        ("last_odd_poll", ctypes.c_uint8 * 32),
        ("last_odd_poll_length", ctypes.c_uint32),
        ("last_odd_poll_us", ctypes.c_uint64),
    ]


SENSORS = ["uptime", "longest_poll_gap", "longest_loop", "sessions_lost", "mqtt_reconnects", "squares", "odd_polls"]
STATE_TOPIC = b"canvas/ae80721e70bf3563/diagnostics"


@pytest.fixture(scope="module")
def lib(build_host_library):
    library = build_host_library("diagnostics.c")
    library.diagnostics_json.argtypes = [ctypes.POINTER(Diagnostics), ctypes.c_char_p, ctypes.c_size_t]
    library.diagnostics_json.restype = ctypes.c_size_t
    library.diagnostics_sensor_count.restype = ctypes.c_size_t
    library.diagnostics_sensor_id.argtypes = [ctypes.c_size_t]
    library.diagnostics_sensor_id.restype = ctypes.c_char_p
    library.diagnostics_discovery_json.argtypes = [ctypes.c_size_t] + [ctypes.c_char_p] * 5 + [ctypes.c_size_t]
    library.diagnostics_discovery_json.restype = ctypes.c_size_t
    return library


def as_json(lib, diagnostics: Diagnostics) -> dict:
    buffer = ctypes.create_string_buffer(320)
    length = lib.diagnostics_json(ctypes.byref(diagnostics), buffer, len(buffer))
    assert length == len(buffer.value) > 0
    return json.loads(buffer.value)


def test_reports_counters_in_seconds_and_milliseconds(lib):
    diagnostics = Diagnostics(
        uptime_us=86_400_123_456,
        longest_poll_gap_us=480_999,
        longest_loop_us=12_345,
        sessions_lost=3,
        mqtt_reconnects=1,
        squares=13,
        odd_polls=2,
        last_odd_poll_length=4,
        last_odd_poll_us=3_600_000_000,
    )
    diagnostics.last_odd_poll[:4] = [0x00, 0x00, 0xCC, 0x00]
    assert as_json(lib, diagnostics) == {
        "uptime_s": 86400,
        "longest_poll_gap_ms": 480,
        "longest_loop_ms": 12,
        "sessions_lost": 3,
        "mqtt_reconnects": 1,
        "squares": 13,
        "odd_polls": 2,
        "last_odd_poll": "0000CC00",
        "last_odd_poll_uptime_s": 3600,
    }


def test_no_odd_poll_yet_is_an_empty_string(lib):
    assert as_json(lib, Diagnostics(squares=13))["last_odd_poll"] == ""


def test_longest_odd_poll_fits(lib):
    diagnostics = Diagnostics(
        uptime_us=2**63, longest_poll_gap_us=2**63, longest_loop_us=2**63, last_odd_poll_length=32
    )
    diagnostics.last_odd_poll[:] = [0xCC] * 32
    assert as_json(lib, diagnostics)["last_odd_poll"] == "CC" * 32


def discovery(lib, index: int) -> dict:
    buffer = ctypes.create_string_buffer(768)
    length = lib.diagnostics_discovery_json(
        index,
        b"canvas_ae80721e70bf3563_sensor_x",
        STATE_TOPIC,
        b"canvas/ae80721e70bf3563/status",
        b"canvas_ae80721e70bf3563",
        buffer,
        len(buffer),
    )
    assert length == len(buffer.value) > 0
    return json.loads(buffer.value)


def test_one_diagnostic_sensor_per_counter(lib):
    assert lib.diagnostics_sensor_count() == len(SENSORS)
    assert [lib.diagnostics_sensor_id(i).decode() for i in range(len(SENSORS))] == SENSORS
    fields = set(as_json(lib, Diagnostics()))
    for index in range(len(SENSORS)):
        payload = discovery(lib, index)
        assert payload["entity_category"] == "diagnostic"
        assert payload["state_topic"] == STATE_TOPIC.decode()
        assert payload["device"]["identifiers"] == ["canvas_ae80721e70bf3563"]
        field = payload["value_template"].removeprefix("{{ value_json.").removesuffix(" }}")
        assert field in fields


def test_odd_poll_sensor_carries_the_last_reply_as_attributes(lib):
    payload = discovery(lib, SENSORS.index("odd_polls"))
    assert payload["json_attributes_topic"] == STATE_TOPIC.decode()
    assert "value_json.last_odd_poll" in payload["json_attributes_template"]
    assert "json_attributes_topic" not in discovery(lib, SENSORS.index("uptime"))


def test_durations_have_units(lib):
    assert discovery(lib, SENSORS.index("uptime"))["unit_of_measurement"] == "s"
    assert discovery(lib, SENSORS.index("longest_loop"))["unit_of_measurement"] == "ms"


def test_rejects_unknown_sensor_and_small_buffer(lib):
    buffer = ctypes.create_string_buffer(768)
    assert lib.diagnostics_discovery_json(len(SENSORS), b"u", b"s", b"a", b"d", buffer, len(buffer)) == 0
    small = ctypes.create_string_buffer(64)
    assert lib.diagnostics_discovery_json(0, b"u", b"s", b"a", b"d", small, len(small)) == 0
