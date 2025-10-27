#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>     // NEW: filesystem support
#include "config.h"
#include "debug_log.h"
// WebServer on port 80
static WebServer http(80);

// Extern scan/telemetry tables
extern int8_t     g_cfgForId[DPMS_SIZE];
extern uint32_t   g_cfgMaskForId[DPMS_SIZE];
extern uint8_t    g_foundIds[DPMS_SIZE];
extern int        g_foundCount;
extern DPMState   dpms[DPMS_SIZE];

// Helpers from modbus_scan.cpp
extern const char* modbus_cfg_name(int idx);
extern int         modbus_cfg_count();

// --------------------------------------------------------------------
// Utility: count bits in mask
// --------------------------------------------------------------------
static uint8_t bitcount(uint32_t x) {
  uint8_t c = 0; while (x) { x &= (x - 1); ++c; } return c;
}

// --------------------------------------------------------------------
// JSON API endpoint: return Modbus status
// --------------------------------------------------------------------
static void handleStatusJson() {
  String out = "{\"cfgNames\":[";
  for (int i = 0; i < modbus_cfg_count(); ++i) {
    if (i) out += ',';
    out += '"'; out += modbus_cfg_name(i); out += '"';
  }
  out += "],\"devices\":[";

  bool first = true;
  for (int i = 0; i < g_foundCount; ++i) {
    uint8_t id = g_foundIds[i];
    if (id == 0 || id >= DPMS_SIZE) continue;

    if (!first) out += ',';
    first = false;

    int8_t cfg = g_cfgForId[id];
    uint32_t mask = g_cfgMaskForId[id];
    bool multi = (bitcount(mask) > 1);

    out += "{\"id\":" + String(id);
    out += ",\"valid\":" + String(dpms[id].valid ? "true":"false");
    out += ",\"cfg\":" + String(cfg);
    out += ",\"cfgName\":\"" + String(modbus_cfg_name(cfg)) + "\"";
    out += ",\"mask\":" + String(mask);
    out += ",\"multi\":" + String(multi ? "true":"false");
    out += ",\"dpm_state\":" + String(dpms[id].dpm_state);
    out += ",\"volt_act\":"  + String(dpms[id].volt_act);
    out += ",\"cur_act\":"   + String(dpms[id].cur_act);
    out += ",\"temp_act\":"  + String(dpms[id].temp_act);
    out += ",\"last_ms\":"   + String(dpms[id].last_ms);
    out += "}";
  }
  out += "]}";

  http.send(200, "application/json", out);
}

// --------------------------------------------------------------------
// Static file serving from LittleFS
// --------------------------------------------------------------------
static String guessContentType(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}

static void handleFile(const String& path) {
  String actual = path;
  if (actual == "/") actual = "/index.html";   // default page
  if (LittleFS.exists(actual)) {
    File f = LittleFS.open(actual, "r");
    http.streamFile(f, guessContentType(actual));
    f.close();
  } else {
    http.send(404, "text/plain", "Not found");
  }
}

// --------------------------------------------------------------------
// Init HTTP server
// --------------------------------------------------------------------
void http_begin() {
  // Mount filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed");
  } else {
    Serial.println("[FS] LittleFS mounted");
  }

  http.on("/", []() { handleFile("/index.html"); });
  http.on("/modbus/status.json", handleStatusJson);
  http.on("/api/status", handleStatusJson);   // <-- NEW alias
  http.on("/api/modbus/status", handleStatusJson);
  // Static file fallback
  http.onNotFound([]() {
    handleFile(http.uri());
  });

  http.begin();
}

// --------------------------------------------------------------------
// Poll loop → must be called often in task
// --------------------------------------------------------------------
void http_poll() {
  http.handleClient();
}
