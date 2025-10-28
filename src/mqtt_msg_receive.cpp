#include "mqtt_msg_receive.h"
#include "app_settings.h"
#include "config.h"
#include "relay_if.h"
#include "update_mgr.h"
#include "watchdog.h"
#include "mqtt_if.h"
#include "debug_log.h"

// -------------------------------------------------------------------
// External functions defined in other modules
// -------------------------------------------------------------------
extern void saveConfig();

// ===========================================================
// [SECTION] Helper: Jason get Int from JsonArray with default
// ===========================================================
int ja_get_i(const JsonArray &a, size_t i, int defVal)
{
    return (!a.isNull() && i < a.size() && a[i].is<int>()) ? a[i].as<int>() : defVal;
}
// ===========================================================
// [SECTION] MQTT Helper: extract DPM ID from topic suffix, e.g. ".../reset/3" -> 3
// ===========================================================
int extract_dpm_id_from_topic(const char *topic)
{
    if (!topic)
        return -1;
    size_t len = strlen(topic);
    while (len > 0 && topic[len - 1] == '/')
        len--;
    const char *p = topic + len;
    while (p > topic && *(p - 1) != '/')
        p--;
    int id = atoi(p);
    return (id > 0 && id <= ROWS) ? id : -1;
}
// ===========================================================
// [SECTION MQTT] MQTT payload helpers
// ===========================================================

static inline String payload_to_string(const byte *p, unsigned int len)
{
    String s;
    s.reserve(len);
    for (unsigned i = 0; i < len; ++i)
        s += (char)p[i];
    return s;
}

static inline String trim_trailing_slash(String s)
{
    while (s.length() && s.endsWith("/"))
        s.remove(s.length() - 1);
    return s;
}

// ===========================================================
// HANDLER DECLARATIONS
// ===========================================================
static bool handle_user(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_reset(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_curve(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_mode(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_line(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_topic(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_settings(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
static bool handle_ota(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc);
// ===========================================================
// [SECTION MQTT Receive] Message Topic Dispatcher
// ===========================================================
struct TopicHandler
{
    const char *key;
    bool (*fn)(const char *, byte *, unsigned int, DynamicJsonDocument &);
};

static TopicHandler handlers[] = {
    {"/cmd/user/", handle_user},
    {"/cmd/reset", handle_reset},
    {"/cmd/curve", handle_curve},
    {"/cmd/mode", handle_mode},
    {"/cmd/line/", handle_line},
    {"/cmd/Set_Topic", handle_topic},
    {"/cmd/settings", handle_settings},
    {"/cmd/ota", handle_ota},
};
void dpms_apply_transitions(uint8_t beforeMask, uint8_t afterMask)
{
    const int n = (ROWS < 8) ? ROWS : 8;
    for (int i = 0; i < n; ++i)
    {
        const bool wasOn = (beforeMask >> i) & 1u;
        const bool nowOn = (afterMask >> i) & 1u;
        if (wasOn == nowOn)
            continue;

        int id = i + 1; // relay index → dpms index

        if (nowOn)
        {
            dpms[id].state = DPMState::Status::WAIT_CURRENT; // ✅ change wait state
            DBG_INFO("[DPM] Relay %d ON → state=Wait Current\n", id);
        }
        else
        {
            dpms[id].state = DPMState::Status::DPM_OFF; // ✅ change to off state
            DBG_INFO("[DPM] Relay %d OFF → state=DPM_OFF\n", id);
        }
    }
}
// ===========================================================
// [SECTION MQTT Receive] MAIN MQTT MESSAGE switch Relay
// ===========================================================
void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    uint8_t before = relay_if_read_mask();

    // --- 1️⃣ Relay control ---
    if (relay_if_mqtt_handle(mqtt, String(App::BASE_TOPIC), DEVICE_HOST,
                             topic, payload, length))
    {
        uint8_t after = relay_if_read_mask();
        dpms_apply_transitions(before, after);
        mqtt_publish_status(true);
        return;
    }

    // --- 2️⃣ Try to parse JSON ---
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload, length); // tolerant parse

    
  // --- 3️⃣ Dispatch ---
    for (auto &h : handlers)
    {
        if (strstr(topic, h.key))
        {
            if (h.fn(topic, payload, length, doc))
                return;
        }
    }
}

// ===========================================================
//  [SECTION MQTT Receive] Change User HANDLERS
// ===========================================================

static bool handle_user(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{   
    int id = doc["id"] | extract_dpm_id_from_topic(topic);
    if (id < 1 || id > ROWS)
        return true;

    int val = 0;
    if (doc.containsKey("user"))
        val = doc["user"].as<int>();
    else if (length > 0)
    {
        String s = payload_to_string(payload, length);
        s.trim();
        if (s.length())
            val = s.toInt();
    }

    if (val > 0)
        dpms[id].user = val;
    saveConfig();
    mqtt_publish_event("User", dpms[id].user, id, "User Change", "User changed");
    DBG_INFO("[MQTT] DPM%d user set to: %d\n", id, dpms[id].user);
    return true;
}
// ===========================================================
// [SECTION MQTT Receive] Reset Energy Counters HANDLER
// ===========================================================
static bool handle_reset(const char *topic, byte *payload,
                         unsigned int length, DynamicJsonDocument &doc)
{ 

    int id = doc["id"] | extract_dpm_id_from_topic(topic);
    if (id < 1 || id > ROWS)
        return true;

    String target = doc["target"] | "";

    if (target.equalsIgnoreCase("anode"))
    {
        dpms[id].energy_anode = 0;
        mqtt_publish_event("Anode", dpms[id].user, id, "User Change", "Reset Anode");
        DBG_INFO("[EVENT] DPM%d anode reset by user %d\n", id, dpms[id].user);
    }
    else if (target.equalsIgnoreCase("total"))
    {
        dpms[id].energy_total = 0;
        mqtt_publish_event("Total Counter", dpms[id].user, id, "User Change", "Reset Total");
        DBG_INFO("[EVENT] DPM%d total energy reset by user %d\n", id, dpms[id].user);
    }
    else if (target.equalsIgnoreCase("temp"))
    {
        dpms[id].energy_temp = 0;
        mqtt_publish_event("Temp Counter", dpms[id].user, id, "User Change", "Reset Temp");
        DBG_INFO("[EVENT] DPM%d temp energy reset by user %d\n", id, dpms[id].user);
    }
    else
    {
        DBG_INFO("[MQTT] Unknown reset target: %s\n", target.c_str());
    }

    DBG_INFO("[MQTT] DPM%d reset target=%s\n", id, target.c_str());
    return true;
}

// ===========================================================
// [SECTION MQTT Receive] Change Curve Mode HANDLER
// ===========================================================
static bool handle_mode(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{    
    int id = doc["id"] | extract_dpm_id_from_topic(topic);
    if (id < 1 || id > ROWS)
        return true;

    String modeStr = doc["mode"] | "Normal";
    modeStr.trim();
    uint8_t curve_mode = 0;
    if (modeStr.equalsIgnoreCase("Ramp"))
        curve_mode = 1;
    if (modeStr.equalsIgnoreCase("Pulse"))
        curve_mode = 2;
    dpms[id].curve_mode = curve_mode;

    DBG_INFO("[MQTT] DPM%d curve_mode set to %s (%u)\n",
             id, modeStr.c_str(), curve_mode);
    mqtt_publish_event("Mode Set", dpms[id].user, id, "User Change", modeStr.c_str());
    return true;
}
// ===========================================================
// [SECTION MQTT Receive] Change Current Ramp/Curve HANDLER
// ===========================================================
static bool handle_curve(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{    
    int id = doc["id"] | extract_dpm_id_from_topic(topic);
    if (id < 1 || id > ROWS)
        return true;

    DPMState &d = dpms[id];
    d.curve.active = false;
    d.curve.type = doc["type"] | 0;

    switch (d.curve.type)
    {
    case 1:
        d.curve.start_cur = doc["start"] | d.cur_set;
        d.curve.end_cur = doc["end"] | d.cur_set;
        d.curve.duration = doc["duration"] | 5000;
        d.curve.active = true;
        DBG_INFO("[MQTT] DPM%d curve LinearRamp %d→%d in %lu ms\n",
                 id, d.curve.start_cur, d.curve.end_cur, d.curve.duration);
        break;
    case 2:
        d.curve.pulse_high = doc["high"] | d.cur_set;
        d.curve.pulse_low = doc["low"] | d.idle_cur;
        d.curve.pulse_period = doc["period"] | 2000;
        d.curve.active = true;
        DBG_INFO("[MQTT] DPM%d curve Pulse hi=%d lo=%d period=%lu\n",
                 id, d.curve.pulse_high, d.curve.pulse_low, d.curve.pulse_period);
        break;
    default:
        d.curve.type = 0;
        d.curve.active = false;
        DBG_INFO("[MQTT] DPM%d curve disabled\n", id);
        break;
    }
    return true;
}
// ===========================================================
// [SECTION MQTT Receive] Change Line (Galvanic id) HANDLER
// ===========================================================
static bool handle_line(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{   
    int id = doc["id"] | extract_dpm_id_from_topic(topic);
    if (id < 1 || id > ROWS)
        return true;

    if (doc.containsKey("line"))
        dpms[id].line_id = doc["line"].as<int>();
    else if (length > 0)
    {
        String s = payload_to_string(payload, length);
        s.trim();
        if (s.length())
            dpms[id].line_id = s.toInt();
    }

    mqtt_publish_event("Line", dpms[id].line_id, id, "User Change", "Neue Linie");
    DBG_INFO("[MQTT] DPM%d line set to %d\n", id, dpms[id].line_id);
    return true;
}
// ====================================================================
// [SECTION MQTT Receive] Set (Sub) Topic HANDLER  DPM_Control/SubTopic
// ====================================================================
static bool handle_topic(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{    
    String newAlias = doc["Topic"] | "";
    if (newAlias.isEmpty())
    {
        DBG_INFO("[MQTT] Missing Topic\n");
        return true;
    }

    Preferences prefs;
    prefs.begin("dpm_cfg", false);
    prefs.putString("alias", newAlias);
    prefs.end();
    DEVICE_HOST = newAlias;
    mqtt_publish_event("alias_set", 0, 0, "system", newAlias.c_str());
    DBG_INFO("[MQTT] Topic changed to: %s\n", newAlias.c_str());

    // Optional: trigger reconnect
    mqtt_disconnect();
    delay(500);
    mqtt_try_connect_with_backoff();
    return true;
}

// ===============================================================
// [SECTION MQTT Receive] DPM settings Time/A/V and log all changes
// ===============================================================
static bool handle_settings(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{
    if (!doc["data"].is<JsonArray>())
        return false;

    JsonArray data = doc["data"].as<JsonArray>();
    const int addr = ja_get_i(data, 0, -1);
    const int volt_set = ja_get_i(data, 1, -1);
    const int cur_set = ja_get_i(data, 2, -1);
    const int runtime = ja_get_i(data, 3, -1);
    const int idle_cur = ja_get_i(data, 4, -1);
    const int percent = ja_get_i(data, 5, -1);

    if (addr < 0 || addr >= ROWS)
        return false;

    const int dpmi = addr + 1; // web 0-based → dpms[1..ROWS]
    String user = doc["user"] | "unknown";
    // --- Detect and log parameter changes ---
    auto logChange = [&](const char *param, int oldVal, int newVal)
    {
        if (oldVal != newVal)
        {
            String topicInflux = String(App::BASE_TOPIC) + "/" + DEVICE_HOST + "/influx";
            String payload = "event,device=" + DEVICE_HOST +
                             ",dpm=" + String(dpmi) +
                             ",type=param_change,field=" + param +
                             ",user=" + dpms[dpmi].user +
                             " old=" + String(oldVal) +
                             ",new=" + String(newVal) + "\n";

            mqtt.publish(topicInflux.c_str(), payload.c_str(), false);
            DBG_INFO("[EVENT] DPM%d: %s changed %s from %d → %d\n",
                     dpmi, user.c_str(), param, oldVal, newVal);
        }
    };

    logChange("volt_set", dpms[dpmi].volt_set, volt_set);
    logChange("cur_set", dpms[dpmi].cur_set, cur_set);
    logChange("runtime", dpms[dpmi].runtime, runtime);
    logChange("idle_cur", dpms[dpmi].idle_cur, idle_cur);

    // --- Apply new settings ---
    if (volt_set >= 0 && volt_set <= 2000)//Radix point 2 (20Volt max)
        (void)dpm_write_voltage((uint8_t)dpmi, (uint16_t)volt_set);
    if (cur_set >= 0 && cur_set <= 20000)//Radix point 3 (20Amp max)
        (void)dpm_write_current((uint8_t)dpmi, (uint16_t)cur_set);
    (void)dpm_write_state((uint8_t)dpmi, true);
    if (volt_set >= 0 && volt_set <= 2000)//Radix point 2(20Volt max)
        dpms[dpmi].volt_set = volt_set;
    if (cur_set >= 0 && cur_set <= 20000)//Radix point 3 (20Amp max)
        dpms[dpmi].cur_set = cur_set;
    if (runtime >= 0)
        dpms[dpmi].runtime = runtime;
    if (idle_cur >= 0)
        dpms[dpmi].idle_cur = idle_cur;
    if (percent >= 0)
        dpms[dpmi].percent = percent;

    saveConfig();
    mqtt_publish_event("Change DPM Settings", dpms[dpmi].user, dpmi, "User Change", "Settings Changed");
    return true;
}
// ===============================================================
// [SECTION MQTT Receive] OTA firmware update
// ===============================================================
static bool handle_ota(const char *topic, byte *payload, unsigned int length, DynamicJsonDocument &doc)
{   
    DBG_INFO("[MQTT] OTA command received on topic: %s\n", topic);

    // Accept both top-level or nested "ota"
    JsonObject o;
    if (doc.containsKey("ota") && doc["ota"].is<JsonObject>())
        o = doc["ota"].as<JsonObject>();
    else
        o = doc.as<JsonObject>();

    String urlS = o["url"] | "";
    String md5S = o["md5"] | "";
    bool reboot = o["reboot"] | false;

    if (urlS.isEmpty())
    {
        DBG_INFO("[MQTT] OTA ignored (missing URL)\n");
        return true;
    }

    DBG_INFO("[MQTT] OTA start: url=%s, md5=%s, reboot=%s\n",
             urlS.c_str(),
             md5S.c_str(),
             reboot ? "true" : "false");

    update_mgr_begin(urlS.c_str(),
                     md5S.length() ? md5S.c_str() : nullptr,
                     reboot);

    return true;
}
