#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mqtt_if.h" 

// Forward declarations from other modules
extern void dpms_apply_transitions(uint8_t beforeMask, uint8_t afterMask);
extern bool mqtt_publish_event(const char *type, int user, int dpm,
                               const char *state, const char *message);
extern bool mqtt_publish_status(bool retained);
extern void saveConfig();

// Public entry point for MQTT message callback
void onMqttMessage(char *topic, byte *payload, unsigned int length);

// Utility: extract DPM number from topic suffix ".../3" -> 3
int extract_dpm_id_from_topic(const char *topic);
