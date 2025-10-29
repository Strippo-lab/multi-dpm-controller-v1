#include "mqtt_if.h"
#include "app_settings.h"
#include "config.h"
#include "pins.h"
#include <math.h>       // isfinite, roundf
#include <esp_system.h> // esp_fill_random()
#include <Arduino.h>    // millis(), String
#include <Ethernet.h>   // W5500 via Arduino Ethernet lib
#include <PubSubClient.h>
#include <Preferences.h> // NVS for UUID
#include <cstdio>
#include "watchdog.h"
#include "update_mgr.h"
#include "net_client.h"
#include "relay_if.h"
#include "mqtt_msg_receive.h"
#include "debug_log.h"

// -------------------------------------------------------------------
// Global network client instance
// -------------------------------------------------------------------
Client *gNetClient = nullptr;
QueueHandle_t qMqttPublish = nullptr; // actual definition
// -------------------------------------------------------------------
// MQTT transport + client
// -------------------------------------------------------------------
static EthernetClient eth_net; // underlying transport
PubSubClient mqtt(eth_net);    // PubSubClient on top of Ethernet

// -------------------------------------------------------------------
// MQTT topics (set in mqtt_init())
// -------------------------------------------------------------------
String T_CMD, T_CONF, T_STAT, T_EVENT, T_LWT;

// -------------------------------------------------------------------
// Device identity
// -------------------------------------------------------------------
static String HOSTNAME;       // esp-<mac>
static String MQTT_CLIENT_ID; // <= 23 chars
String DEVICE_HOST;           // namespace for topics

// -------------------------------------------------------------------
// Connection state
// -------------------------------------------------------------------
static uint32_t g_mqttConnectedMs = 0;
static bool g_bootBurstPending = false;
static uint8_t g_pubFailCount = 0;

// -------------------------------------------------------------------
// Helper: check if topic == "<ns>/<DEVICE_HOST>/settings"
// -------------------------------------------------------------------
static bool topic_is_my_settings(const char *topic)
{
    const char *s1 = strchr(topic, '/');
    if (!s1)
        return false;
    const char *s2 = strchr(s1 + 1, '/');
    if (!s2)
        return false;
    if (strcmp(s2 + 1, "settings") != 0)
        return false;

    const size_t segLen = static_cast<size_t>(s2 - (s1 + 1));
    if (segLen == 0)
        return false;

    const char *host = DEVICE_HOST.c_str();
    return strncmp(s1 + 1, host, segLen) == 0 && host[segLen] == '\0';
}

// -------------------------------------------------------------------
// Helper: safe array getter
// -------------------------------------------------------------------
static int ja_get_i(const JsonArray &a, size_t i, int defVal)
{
    return (!a.isNull() && i < a.size() && a[i].is<int>()) ? a[i].as<int>() : defVal;
}

// -------------------------------------------------------------------
// Utility: generate MAC string
// -------------------------------------------------------------------
static String mac_hex12()
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
             (uint8_t)(mac >> 40), (uint8_t)(mac >> 32), (uint8_t)(mac >> 24),
             (uint8_t)(mac >> 16), (uint8_t)(mac >> 8), (uint8_t)(mac));
    return String(buf);
}
// -------------------------------------------------------------------
// Utility: load host alias name or use MAC as hostname
// -------------------------------------------------------------------
String get_device_host()
{
    Preferences prefs;
    // Try to open existing namespace
    if (!prefs.begin("dpm_cfg", true))
    {
        // Namespace not found → create it
        prefs.begin("dpm_cfg", false);
        prefs.end();
        prefs.begin("dpm_cfg", true);
    }
    String alias = prefs.getString("alias", "");
    prefs.end();

    if (alias.length() > 0)
    {
        return alias;
    }

    return "DPM" + mac_hex12();
}
// -------------------------------------------------------------------
// Connectivity check
// -------------------------------------------------------------------
static inline bool eth_connected()
{
    return Ethernet.linkStatus() == LinkON &&
           Ethernet.localIP() != IPAddress(0, 0, 0, 0);
}
bool mqtt_connected() { return mqtt.connected(); }

// -------------------------------------------------------------------
// Generic JSON publisher (used for status/config/event)
// -------------------------------------------------------------------
static inline bool publishJson(const String &topic, JsonDocument &doc, bool retained)
{
    if (!mqtt.connected())
        return false;

    char buf[1024];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    bool ok = mqtt.publish(topic.c_str(), (const uint8_t *)buf, n, retained);

    if (ok)
        DBG_INFO("[PUB OK] ✅ %s (%u bytes)\n", topic.c_str(), (unsigned)n);
    else
        DBG_ERROR("[PUB FAIL] ❌ %s (%u bytes)\n", topic.c_str(), (unsigned)n);
    return ok;
}

// -------------------------------------------------------------------
// Publishers: config, status, influx, event
// -------------------------------------------------------------------
// ===========================================================
// [SECTION MQTT Publish] Config line protocol publisher
// ===========================================================
bool mqtt_publish_config()
{
    if (!mqtt_connected())
        return false;
    DynamicJsonDocument doc(4096); // adjust if ROWS grows

    for (int id = 1; id <= ROWS; id++)
    {
        char key[8];
        snprintf(key, sizeof(key), "DPM%d", id);
        JsonArray arr = doc[key].to<JsonArray>();

        arr.add(static_cast<int>(dpms[id].state)); // ✅ cast enum class to int
        arr.add(dpms[id].dpm_state);
        arr.add(dpms[id].volt_act);
        arr.add(dpms[id].cur_act);
        arr.add(dpms[id].temp_act);
        arr.add(dpms[id].remain_time);
        arr.add(dpms[id].volt_set);
        arr.add(dpms[id].cur_set);
        arr.add(dpms[id].idle_cur);
        arr.add(dpms[id].last_ms);
        arr.add(dpms[id].runtime);
        arr.add(dpms[id].user);
        arr.add(dpms[id].line_id);
    }
    return publishJson(T_CONF, doc, true);
}
// ===========================================================
// [SECTION MQTT Publish] Status line protocol publisher
// ===========================================================
bool mqtt_publish_status(bool retained)
{
    if (!mqtt_connected())
        return false;
    DynamicJsonDocument doc(4096); // adjust if ROWS grows

    for (int id = 1; id <= ROWS; id++)
    {
        char key[8];
        snprintf(key, sizeof(key), "DPM%d", id);
        JsonArray arr = doc[key].to<JsonArray>();

        arr.add(static_cast<int>(dpms[id].state)); // ✅ cast enum class to int
        arr.add(dpms[id].dpm_state);
        arr.add(dpms[id].volt_act);
        arr.add(dpms[id].cur_act);
        arr.add(dpms[id].temp_act);
        arr.add(dpms[id].remain_time);
        arr.add(dpms[id].volt_set);
        arr.add(dpms[id].cur_set);
        arr.add(dpms[id].idle_cur);
        arr.add(dpms[id].last_ms);
        arr.add(dpms[id].runtime);
        arr.add(dpms[id].energy_temp);
        arr.add(dpms[id].energy_total);
        arr.add(dpms[id].energy_anode);
        arr.add(dpms[id].user);
        // arr.add(dpms[id].curve.type); // curve type
    }
    return publishJson(T_STAT, doc, retained);
}
// ===========================================================
// [SECTION MQTT Publish] Influx line protocol publisher
// ===========================================================

bool mqtt_publish_influx()
{

    if (!mqtt_connected())
        return false;
        

    static String payload;
    payload.reserve(1024);
    payload.remove(0); // clear string, keep capacity

    for (int id = 1; id <= ROWS; id++)
    {
        if (dpms[id].valid && dpms[id].state == DPMState::Status::RUN)
        { // ✅ updated

            // Build one line of Influx line protocol
            payload += F("dpm,device=");
            payload += DEVICE_HOST;
            payload += F(",dpm=");
            payload += id;
            payload += F(" volt=");
            payload += dpms[id].volt_act;
            payload += F(",curr=");
            payload += dpms[id].cur_act;
            payload += F(",temp=");
            payload += dpms[id].temp_act;
            payload += ",energy_total=";
            payload += dpms[id].energy_total;
            payload += ",energy_anode=";
            payload += dpms[id].energy_anode;
            payload += ",energy_temp=";
            payload += dpms[id].energy_temp;
            payload += ",user=";
            payload += dpms[id].user;
            payload += '\n'; // newline terminator
        }
    }

    if (payload.isEmpty())
        return false;

    String topic = String(App::BASE_TOPIC) + "/" + DEVICE_HOST + "/influx";
    bool ok = mqtt.publish(topic.c_str(), payload.c_str(), false);

    if (ok)
        DBG_INFO("[PUB OK] ✅ %s (%u bytes)\n", topic.c_str(), payload.length());
    else
        DBG_WARN("[PUB FAIL] ❌ %s (%u bytes)\n", topic.c_str(), payload.length());
    return ok;
}

// ===========================================================
// [SECTION MQTT Publish] Event line protocol publisher
// ===========================================================
bool mqtt_publish_event(const char *type, int user, int dpm,
                        const char *state, const char *message)
{
    if (!mqtt_connected())
        return false;

    StaticJsonDocument<512> doc;

    // --- Core metadata ---
    // "line" replaces old "cluster"
    if (dpm >= 1 && dpm <= ROWS && ROWS > 0)
    {
        int lineId = dpms[dpm].line_id;
        char lineName[16];
        snprintf(lineName, sizeof(lineName), "Line %d", lineId);
        doc["cluster"] = lineName; // e.g. "Line 1"
    }
    else
    {
        doc["cluster"] = "Unknown";
    }

    doc["device"] = DEVICE_HOST; // e.g. "Cluster10"
    doc["dpm"] = dpm;            // DPM number
    doc["type"] = type;          // event type (relay_switched, etc.)
    doc["user"] = user;          // numeric user id

    // --- Optional fields ---
    if (state && *state)
        doc["state"] = state;
    if (message && *message)
        doc["message"] = message;

    // --- Build topic ---
    String topic = String(App::BASE_TOPIC) + "/" + DEVICE_HOST + "/event";

    // --- Serialize & publish ---
    char buf[512];
    size_t n = serializeJson(doc, buf, sizeof(buf));

    bool ok = mqtt.publish(topic.c_str(), (const uint8_t *)buf, n, false);

    if (ok)
        DBG_INFO("[MQTT_Send_Event] ✅ %s | Line=%s | DPM=%d | State=%s\n",
                 type, doc["cluster"].as<const char *>(), dpm,
                 state ? state : "-");
    else
        DBG_ERROR("[MQTT] ❌ Failed to publish event: %s\n", type);

    return ok;
}

// ===========================================================
// [SECTION MQTT]  Disconnect Handler
// ============================================================

void mqtt_disconnect()
{
    if (mqtt.connected())
    {
        DBG_INFO("[MQTT] ⚠️ Disconnect\n");
        mqtt.disconnect();
    }
}
// ===========================================================
// [SECTION MQTT] Subscribe and Connect Handler
// ===========================================================
void mqtt_try_connect_with_backoff(bool /*immediate*/)
{
    static uint32_t lastAttempt = 0;
    uint32_t now = millis();

    // --- Throttle retry interval ---
    if (now - lastAttempt < 3000)
        return;
    lastAttempt = now;

    // --- Skip if already connected ---
    if (mqtt.connected())
        return;

    // --- Ensure network ready ---
    if (!eth_connected())
    {
        static bool firstMsg = true;
        if (firstMsg)
        {
            DBG_WARN("[MQTT] 💤 Waiting for network connection...\n");
            firstMsg = false;
        }
        return;
    }

    // --- Ensure client ID is valid (avoid rc=-2 on startup) ---
    if (MQTT_CLIENT_ID.isEmpty() || DEVICE_HOST.isEmpty())
    {
        static bool firstMsg = true;
        if (firstMsg)
        {
            DBG_WARN("[MQTT] 💤 Skipping connect — client ID/host not yet set\n");
            firstMsg = false;
        }
        return;
    }

    DBG_INFO("[MQTT] 🔄 Connecting to %s:%u as '%s'...\n",
             App::MQTT_HOST, App::MQTT_PORT, MQTT_CLIENT_ID.c_str());

    mqtt.setKeepAlive(60);
    mqtt.setSocketTimeout(12);

    bool ok = mqtt.connect(
        MQTT_CLIENT_ID.c_str(),
        App::MQTT_USER, App::MQTT_PASS,
        T_LWT.c_str(), 1, true, "offline");

    if (ok)
    {
        DBG_INFO("[MQTT] ✅ Connected.\n");

        // ===========================================================
        // [SECTION MQTT Subscribe] Topics
        // ===========================================================
       
        const String base = String(App::BASE_TOPIC);

        // 1) Subscribe to relay controls (per-channel + batch)
        relay_if_mqtt_subscribe(mqtt, base, DEVICE_HOST);

        bool ok = true;
        ok &= mqtt.subscribe((base + "/" + DEVICE_HOST + "/cmd/#").c_str(), 1);
        ok &= mqtt.subscribe((base + "/" + DEVICE_HOST + "/settings").c_str(), 1); 
        DBG_INFO("[MQTT] subs %s\n", ok ? "OK" : "FAIL");
        
        // 2) Publish LWT 'online' (birth) retained message
        mqtt.publish(T_LWT.c_str(), (const uint8_t *)"online", 6, true);        

        // 3) Announce firmware/version as an event (non-retained)
        mqtt_publish_event("firmware_version", 0, 0, "Boot", FW_VERSION_STRING);
        
        g_mqttConnectedMs = now;
        g_bootBurstPending = true;
    }
    else
    {
        // only print first failure or every 5th try to avoid log spam
        static uint8_t failCount = 0;
        failCount++;
        if (failCount == 1 || (failCount % 5 == 0))
            DBG_WARN("[MQTT] ⚠️ Connect failed (rc=%d), will retry...\n", mqtt.state());
    }
}
// ===========================================================
// [SECTION MQTT] MQTT Init (called once in setup())
// ===========================================================

void mqtt_init()
{

    mqtt.setBufferSize(1024);
    mqtt.setServer(App::MQTT_HOST, App::MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setKeepAlive(60);
    mqtt.setSocketTimeout(12);

    String mac = mac_hex12();
    DEVICE_HOST = get_device_host(); // "TEST_DPM";
    HOSTNAME = "esp-" + mac;
    MQTT_CLIENT_ID = "dpm-" + mac.substring(0, 12);

    const String dev = DEVICE_HOST;
    T_CMD = String(App::BASE_TOPIC) + "/" + dev + "/cmd";
    T_CONF = String(App::BASE_TOPIC) + "/" + dev + "/config";
    T_STAT = String(App::BASE_TOPIC) + "/" + dev + "/status";
    T_EVENT = String(App::BASE_TOPIC) + "/" + dev + "/event";
    T_LWT = String(App::BASE_TOPIC) + "/" + dev + "/lwt";
    DBG_INFO("[ID] ✅ HOST=%s CID=%s\n", HOSTNAME.c_str(), MQTT_CLIENT_ID.c_str());
}
// ==================================================================================
// [SECTION MQTT Publish] Centralized MQTT task (only this task calls mqtt.publish())
// ==================================================================================
static void mqttTask(void *)
{
    for (;;)
    {
        watchdog_feed();

        if (!eth_connected())
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (!mqtt.connected())
        {
            mqtt_try_connect_with_backoff(false);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        mqtt.loop();

        MqttMsg msg;
        if (xQueueReceive(qMqttPublish, &msg, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            bool ok = false;
            switch (msg.type)
            {
            case MSG_STATUS:
                ok = mqtt_publish_status(msg.retained);
                break;
            case MSG_INFLUX:
                ok = mqtt_publish_influx();
                break;
            case MSG_CONFIG:
                ok = mqtt_publish_config();
                break;
            case MSG_EVENT:
                const char *eventType = msg.eventType ? msg.eventType : "unknown";
                const char *stateStr = msg.state ? msg.state : "";    // optional state
                const char *msgText = msg.message ? msg.message : ""; // optional message/description

                ok = mqtt_publish_event(
                    eventType,
                    msg.user, // numeric user id
                    msg.id,   // DPM number
                    stateStr,
                    msgText);
                break;
            }
            if (!ok)
            {
                g_pubFailCount++;
                if (g_pubFailCount > 5)
                {
                    DBG_WARN("[MQTT] too many publish fails, reconnect\n");
                    mqtt_disconnect();
                    g_pubFailCount = 0;
                }
            }
            else
            {
                g_pubFailCount = 0;
            }
        }

        if (g_bootBurstPending && (millis() - g_mqttConnectedMs) > 800)
        {
            mqtt_publish_config();
            mqtt_publish_status(true);
            mqtt_publish_event(
                "connect",                          // type
                1,                                  // user (system or initial user)
                1,                                  // DPM number (1 = DPM1)
                "OK",                               // state: "OK", "CONNECTED", or "ONLINE"
                "Device connected to MQTT broker"); // message
            g_bootBurstPending = false;
        }
    }
}

// -------------------------------------------------------------------
// [SECTION MQTT] Public API for other tasks (enqueue publish requests)
// -------------------------------------------------------------------
void mqtt_request_status(bool retained)
{
    MqttMsg msg = {MSG_STATUS, retained, nullptr, 0, 0};
    if (xQueueSend(qMqttPublish, &msg, 0) != pdTRUE)
        DBG_WARN("[MQTT] queue full, dropped STATUS\n");
}

void mqtt_request_influx()
{
    MqttMsg msg = {MSG_INFLUX, false, nullptr, 0, 0};
    if (xQueueSend(qMqttPublish, &msg, 0) != pdTRUE)
        DBG_WARN("[MQTT] queue full, dropped INFLUX\n");
}
void mqtt_request_config()
{
    MqttMsg msg = {MSG_CONFIG, true, nullptr, 0, 0};
    if (xQueueSend(qMqttPublish, &msg, 0) != pdTRUE)
        DBG_WARN("[MQTT] queue full, dropped CONFIG\n");
}
void mqtt_request_event(const char *type, int user, int id)
{
    MqttMsg msg = {MSG_EVENT, false, type, user, id};
    if (xQueueSend(qMqttPublish, &msg, 0) != pdTRUE)
    {
        DBG_WARN("[MQTT] queue full, dropped EVENT\n");
    }
}
// -------------------------------------------------------------------
// [SECTION MQTT] Start the MQTT task (call from start_system_tasks())
// -------------------------------------------------------------------
void start_mqtt_task()
{
    xTaskCreatePinnedToCore(mqttTask, "mqttTask", 12288, nullptr, 3, nullptr, 1);
}
