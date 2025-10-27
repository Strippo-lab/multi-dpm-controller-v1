#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <Wire.h>
// Init the TCA9554 expander and reset all relays OFF.
// Requires Wire.begin(42,41,100000) to have been called already.
void relay_if_init();

// Optional: invert polarity in software (default false / active-HIGH).
void relay_if_set_inverted(bool inverted);

// Programmatic control
bool relay_if_set(uint8_t relay, bool on);   // relay 1..8
bool relay_if_toggle(uint8_t relay);
bool relay_if_write_mask(uint8_t mask);      // bit=1 => ON
uint8_t relay_if_read_mask();                // cached shadow, not a register read

// MQTT glue (topics derived from base + device)
// Subscribes to:
//   <base>/<dev>/relay/<n>/set  (n=1..8)   payload: ON/OFF/1/0/TOGGLE
//   <base>/<dev>/relays/set     payload: 0..255 or 0x00..0xFF
void relay_if_mqtt_subscribe(PubSubClient& mqtt,
                             const String& baseTopic,
                             const String& deviceHost);

// Publish retained states to:
//   <base>/<dev>/relay/<n>      payload: ON/OFF
void relay_if_mqtt_publish_states(PubSubClient& mqtt);

// Route incoming MQTT messages; returns true if handled.
bool relay_if_mqtt_handle(PubSubClient& mqtt,
                          const String& baseTopic,
                          const String& deviceHost,
                          char* topic, byte* payload, unsigned int len);
