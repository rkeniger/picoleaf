#include "diagnostics.h"

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const char *id;
    const char *name;
    const char *field;
    // Extra discovery keys, starting with a comma.
    const char *extra;
    // Carries the last odd poll reply as attributes, so it doesn't need a sensor of its own.
    bool odd_poll_attributes;
} sensor_t;

static const sensor_t SENSORS[] = {
    {"uptime", "Uptime", "uptime_s", ",\"device_class\":\"duration\",\"unit_of_measurement\":\"s\"", false},
    {"longest_poll_gap", "Longest poll gap", "longest_poll_gap_ms",
     ",\"device_class\":\"duration\",\"unit_of_measurement\":\"ms\",\"state_class\":\"measurement\"", false},
    {"longest_loop", "Longest main-loop pass", "longest_loop_ms",
     ",\"device_class\":\"duration\",\"unit_of_measurement\":\"ms\",\"state_class\":\"measurement\"", false},
    {"sessions_lost", "Sessions lost", "sessions_lost", ",\"state_class\":\"total_increasing\"", false},
    {"mqtt_reconnects", "MQTT reconnects", "mqtt_reconnects", ",\"state_class\":\"total_increasing\"", false},
    {"squares", "Squares on the bus", "squares", ",\"state_class\":\"measurement\"", false},
    {"odd_polls", "Odd poll replies", "odd_polls", ",\"state_class\":\"total_increasing\"", true},
};

#define SENSOR_COUNT (sizeof SENSORS / sizeof SENSORS[0])

static size_t checked_length(int written, size_t size) {
    return written < 0 || (size_t)written >= size ? 0 : (size_t)written;
}

size_t diagnostics_json(const diagnostics_t *diagnostics, char *out, size_t size) {
    char hex[2 * DIAGNOSTICS_ODD_POLL_MAX + 1];
    size_t length = diagnostics->last_odd_poll_length;
    if (length > DIAGNOSTICS_ODD_POLL_MAX) length = DIAGNOSTICS_ODD_POLL_MAX;
    for (size_t i = 0; i < length; i++) snprintf(hex + 2 * i, 3, "%02X", diagnostics->last_odd_poll[i]);
    hex[2 * length] = '\0';
    int written = snprintf(out, size,
                           "{\"uptime_s\":%llu,\"longest_poll_gap_ms\":%llu,\"longest_loop_ms\":%llu,"
                           "\"sessions_lost\":%lu,\"mqtt_reconnects\":%lu,\"squares\":%lu,\"odd_polls\":%lu,"
                           "\"last_odd_poll\":\"%s\",\"last_odd_poll_uptime_s\":%llu}",
                           (unsigned long long)(diagnostics->uptime_us / 1000000u),
                           (unsigned long long)(diagnostics->longest_poll_gap_us / 1000u),
                           (unsigned long long)(diagnostics->longest_loop_us / 1000u),
                           (unsigned long)diagnostics->sessions_lost, (unsigned long)diagnostics->mqtt_reconnects,
                           (unsigned long)diagnostics->squares, (unsigned long)diagnostics->odd_polls, hex,
                           (unsigned long long)(diagnostics->last_odd_poll_us / 1000000u));
    return checked_length(written, size);
}

size_t diagnostics_sensor_count(void) {
    return SENSOR_COUNT;
}

const char *diagnostics_sensor_id(size_t index) {
    return index < SENSOR_COUNT ? SENSORS[index].id : "";
}

size_t diagnostics_discovery_json(size_t index, const char *unique_id, const char *state_topic,
                                  const char *availability_topic, const char *device_id, char *out, size_t size) {
    if (index >= SENSOR_COUNT) return 0;
    const sensor_t *sensor = &SENSORS[index];
    static char attributes[320];  // static: keeps the discovery call chain small on the 4 KB main stack
    attributes[0] = '\0';
    if (sensor->odd_poll_attributes) {
        int length = snprintf(attributes, sizeof attributes,
                              ",\"json_attributes_topic\":\"%s\",\"json_attributes_template\":"
                              "\"{{ {'last_odd_poll': value_json.last_odd_poll, 'last_odd_poll_uptime_s': "
                              "value_json.last_odd_poll_uptime_s} | tojson }}\"",
                              state_topic);
        if (checked_length(length, sizeof attributes) == 0) return 0;
    }
    int written = snprintf(out, size,
                           "{\"name\":\"%s\",\"unique_id\":\"%s\",\"state_topic\":\"%s\","
                           "\"value_template\":\"{{ value_json.%s }}\",\"availability_topic\":\"%s\","
                           "\"entity_category\":\"diagnostic\"%s%s,\"device\":{\"identifiers\":[\"%s\"]}}",
                           sensor->name, unique_id, state_topic, sensor->field, availability_topic, sensor->extra,
                           attributes, device_id);
    return checked_length(written, size);
}
