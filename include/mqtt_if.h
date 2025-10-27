#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
// -------------------------------------------------------------------
// Message queue types
// -------------------------------------------------------------------
enum MqttMsgType
{
    MSG_STATUS,
    MSG_INFLUX,
    MSG_CONFIG,
    MSG_EVENT
};

struct MqttMsg
{
    MqttMsgType type;     // Message type (e.g. MSG_EVENT, MSG_CMD, etc.)
    bool retained;        // MQTT retain flag

    // --- Event-specific fields ---
    const char *eventType; // "relay_switched", "service_due", "run_start", etc.
    int user;              // user ID (integer, not string)
    int id;                // DPM number (1–8)
    const char *state;     // optional: "ON", "OFF", "RUN", "STOP", etc.
    const char *message;   // optional: human-readable text, may be NULL

    // --- Optional: future extensibility ---
    const char *cluster;   // optional: "Cluster01" (can auto-fill from App::CLUSTER_NAME)
    const char *device;    // optional: "TEST_DPM" (auto from DEVICE_HOST)
};

// -------------------------------------------------------------------
// Get user name for event logging
// -------------------------------------------------------------------
static String g_currentUser = "system"; // Default user name

// Global publish queue (created in start_system_tasks)
extern QueueHandle_t qMqttPublish;
extern PubSubClient mqtt;     // MQTT client instance
extern String DEVICE_HOST;    // Host alias used in topics
extern bool g_modbusScanDone;
// start the centralized mqtt task
void start_mqtt_task();

// regular init/disconnect
void mqtt_init();
void mqtt_disconnect();
bool mqtt_connected();

// Queue-based publish requests (implemented in mqtt_if.cpp)
void mqtt_request_status(bool retained);
void mqtt_request_influx();
void mqtt_request_config();
bool mqtt_publish_status(bool retained = false);
bool mqtt_publish_config();
bool mqtt_publish_influx();
bool mqtt_publish_event(const char *type, int user, int dpm, const char *state, const char *message);

//bool mqtt_publish_event(const char *type, int user, int id);


// Existing public API
void mqtt_init();
void mqtt_disconnect();
void mqtt_try_connect_with_backoff(bool immediate = false);
bool mqtt_connected();
bool mqtt_publish_event(const char *type, int user, int dpm, const char *state, const char *message);





