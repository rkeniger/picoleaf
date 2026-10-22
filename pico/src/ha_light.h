#pragma once

// Home Assistant MQTT "json" schema lights: command parsing, state and
// discovery payloads. Pure C so it runs in the host tests.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool on;
    uint8_t brightness;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t white;  // the squares' white LEDs
    // Colour temperature the colour was mixed from, or 0 when it was set as RGBW.
    uint16_t kelvin;
} light_state_t;

// Colour temperature range offered to Home Assistant. The cool end is the white LEDs alone, the warm
// end those plus amber (90 30 00 FF, picked by eye); neither temperature has been measured.
#define LIGHT_WARM_KELVIN 2700
#define LIGHT_COOL_KELVIN 6500

// After power-up: the warm end of the range at 80% brightness.
#define LIGHT_DEFAULT_STATE                                                                                  \
    ((light_state_t){.on = true, .brightness = 204, .red = 0x90, .green = 0x30, .blue = 0x00, .white = 0xFF, \
                     .kelvin = LIGHT_WARM_KELVIN})

// Applies a command such as {"state":"ON","brightness":80,"color":{"r":255,"g":0,"b":0,"w":0}} or
// {"color_temp":3000} (kelvin, clamped to the range above). Absent fields keep their value, except
// that a "color" without "w" turns the white LEDs off. "effect" and "transition" count as recognised
// but are read with the functions below. Returns false, leaving `state` unchanged, if the payload
// has no recognised field or a value is malformed or out of range.
bool ha_light_apply_command(light_state_t *state, const char *payload, size_t length);

// Copies the "effect" string into `name`. Returns false if absent, malformed or too long.
bool ha_light_command_effect(const char *payload, size_t length, char *name, size_t size);

// Reads "transition" (seconds, may have a fraction) as milliseconds, capped at 10 minutes.
// Returns false if absent or malformed.
bool ha_light_command_transition_ms(const char *payload, size_t length, uint32_t *milliseconds);

// {"state":"ON","brightness":128,"color_mode":"rgbw","color":{"r":255,"g":255,"b":255,"w":0}}, or
// "color_mode":"color_temp","color_temp":<kelvin> when the colour came from a temperature, plus
// "effect":"<name>" when `effect` is not NULL.
// Returns the length written (excluding NUL), or 0 if `size` is too small.
size_t ha_light_state_json(const light_state_t *state, const char *effect, char *out, size_t size);

// Colour sent to the square: RGBW scaled by brightness, black when off.
void ha_light_output(const light_state_t *state, uint8_t rgbw[4]);

typedef struct {
    const char *name;
    const char *unique_id;
    const char *command_topic;
    const char *state_topic;
    const char *availability_topic;
    const char *device_id;
    const char *firmware_version;
    // Optional: advertises "effect":true with these names.
    const char *const *effect_names;
    size_t effect_count;
} ha_light_discovery_t;

// Returns the length written (excluding NUL), or 0 if `size` is too small.
size_t ha_light_discovery_json(const ha_light_discovery_t *light, char *out, size_t size);

// Discovery for the "Layout rotation" select (options 0/90/180/270, entity category config).
// Returns the length written (excluding NUL), or 0 if `size` is too small.
size_t ha_rotation_discovery_json(const char *unique_id, const char *command_topic, const char *state_topic,
                                  const char *availability_topic, const char *device_id, char *out, size_t size);

// Discovery for the "Publish squares" switch (payloads ON/OFF, entity category config).
// Returns the length written (excluding NUL), or 0 if `size` is too small.
size_t ha_publish_squares_discovery_json(const char *unique_id, const char *command_topic, const char *state_topic,
                                         const char *availability_topic, const char *device_id, char *out,
                                         size_t size);
