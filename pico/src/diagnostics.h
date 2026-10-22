#pragma once

// Diagnostic counters for Home Assistant: one JSON message on canvas/<board>/diagnostics and a
// discovery message per sensor. Pure C so it runs in the host tests.

#include <stddef.h>
#include <stdint.h>

#define DIAGNOSTICS_ODD_POLL_MAX 128  // a whole poll reply for 64 squares

// Everything since boot.
typedef struct {
    uint64_t uptime_us;
    uint64_t longest_poll_gap_us;
    uint64_t longest_loop_us;  // the longest main-loop pass, excluding USB commands
    uint32_t sessions_lost;
    uint32_t mqtt_reconnects;
    uint32_t squares;
    uint32_t odd_polls;
    uint8_t last_odd_poll[DIAGNOSTICS_ODD_POLL_MAX];
    uint32_t last_odd_poll_length;
    uint64_t last_odd_poll_us;  // uptime when it arrived
} diagnostics_t;

// {"uptime_s":..,"longest_poll_gap_ms":..,"longest_loop_ms":..,"sessions_lost":..,"mqtt_reconnects":..,
//  "squares":..,"odd_polls":..,"last_odd_poll":"<hex>","last_odd_poll_uptime_s":..}
// Returns the length written (excluding NUL), or 0 if `size` is too small.
size_t diagnostics_json(const diagnostics_t *diagnostics, char *out, size_t size);

size_t diagnostics_sensor_count(void);

// Object ID of sensor `index`, used in its discovery topic and unique ID (e.g. "uptime").
const char *diagnostics_sensor_id(size_t index);

// Discovery for sensor `index`: reads its field from `state_topic` with a value template, entity
// category diagnostic. Returns the length written (excluding NUL), or 0 if `size` is too small or
// `index` is out of range.
size_t diagnostics_discovery_json(size_t index, const char *unique_id, const char *state_topic,
                                  const char *availability_topic, const char *device_id, char *out, size_t size);
