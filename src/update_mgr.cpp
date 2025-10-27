#include "update_mgr.h"
#include "mqtt_if.h"          // mqtt_publish_event(type, pct, retained=false)
#include <Arduino.h>
#include <Update.h>
#include <MD5Builder.h>
#include <cstring>
#include "config.h"
#include "watchdog.h"   // watchdog_feed()
#include <ArduinoHttpClient.h>
#include "debug_log.h"
// ---------- Select your transport ----------
#define USE_ETHERNET 1   // set 1 for W5500, 0 for Wi-Fi

#if USE_ETHERNET
  #include <Ethernet.h>
  static EthernetClient httpSock;
#else
  #include <WiFi.h>
  #include <WiFiClient.h>
  static WiFiClient httpSock;
#endif

// -------------------------------------------------------------------
// OTA event helper — unified MQTT event publisher
// -------------------------------------------------------------------
static inline void ota_evt(const char *type, float pct = 0.0f, const char *msg = nullptr)
{
    char text[64];

    if (pct > 0.0f && pct < 100.0f)
        snprintf(text, sizeof(text), "Progress %.1f%%", pct);
    else if (pct >= 100.0f)
        snprintf(text, sizeof(text), "Complete");
    else if (msg)
        snprintf(text, sizeof(text), "%s", msg);
    else
        snprintf(text, sizeof(text), "Processing");

    // Using user=0, DPM=0 since this is a system-level event
    mqtt_publish_event(type, 0, 0, "OTA", text);
}


// Parse "http://host[:port]/path"
static bool parseHttpUrl(const char* url, String& host, uint16_t& port, String& path) {
  const char* scheme = "http://";
  size_t sl = strlen(scheme);
  if (strncmp(url, scheme, sl) != 0) return false;

  const char* p = url + sl;
  const char* slash = strchr(p, '/');
  String hostport;
  if (slash) { hostport = String(p).substring(0, slash - p); path = String(slash); }
  else       { hostport = String(p);                         path = "/";          }

  int colon = hostport.indexOf(':');
  if (colon >= 0) {
    host = hostport.substring(0, colon);
    port = (uint16_t) hostport.substring(colon + 1).toInt();
    if (port == 0) port = 80;
  } else {
    host = hostport; port = 80;
  }
  return host.length() > 0;
}
// ===========================================================
// [SECTION OTA] Start OTA Update from given URL
// ===========================================================

void update_mgr_begin(const char* url, const char* md5, bool reboot)
{
  if (!url || !url[0]) {
    ota_evt("ota_bad_url", 0, "Bad OTA URL");
    return;
  }

  // Optional MD5 (32 hex)
  char wanted_md5[33] = {0};
  bool want_check = false;

  if (md5 && *md5) {
    size_t n = strlen(md5);

    if (n == 32) {
      strncpy(wanted_md5, md5, 32);
      wanted_md5[32] = 0;
      want_check = true;
    } else {
      // ❌ Invalid MD5 provided → abort OTA early
      ota_evt("ota_md5_invalid", 0, "MD5 invalid length");
      mqtt_publish_event("ota_abort_invalid_md5", 0, 0, "OTA", md5);
      return;   // stop before doing anything
    }
  }

  String host, path;
  uint16_t port = 80;
  if (!parseHttpUrl(url, host, port, path)) {
    ota_evt("ota_bad_url", 0, "Bad OTA URL");
    return;
  }

  if (!path.startsWith("/")) path = "/" + path;

  ota_evt("ota_start", 0, "OTA Start");

  HttpClient http(httpSock, host.c_str(), port);
  http.connectionKeepAlive();

  int err = http.get(path.c_str());
  if (err != 0) {
    ota_evt("ota_http_err", 0, "GET failed");
    http.stop();
    return;
  }

  int status = http.responseStatusCode();
  DBG_INFO("[OTA] HTTP status = %d for %s:%u%s\n",
           status, host.c_str(), port, path.c_str());

  if (status != 200) {
    ota_evt("ota_http_err", 0, ("HTTP error " + String(status)).c_str());
    http.stop();
    return;
  }

  // --- Continue with existing code below (Update.begin, loop, MD5 compare, etc.) ---

  // Content length (-1 if unknown or chunked)
  int total = http.contentLength();
  // Ensure the body is positioned correctly
  http.skipResponseHeaders();

  // Begin Update
  size_t begin_len = (total > 0) ? (size_t)total : (size_t)UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(begin_len)) {
    ota_evt("ota_begin_fail", 0, "Update begin failed");
    http.stop();
    return;
  }

  MD5Builder md5b; if (want_check) md5b.begin();

  const size_t BUF = 4096;
  uint8_t buf[BUF];
  size_t written = 0;
  uint32_t lastMs = 0;

  while (http.connected() || http.available()) {
    int n = http.readBytes((char*)buf, BUF);
    if (n < 0) { // read error
      ota_evt("ota_write_err", 0, "Write error");
      Update.abort();
      http.stop();
      return;
    }
    if (n == 0) { delay(1); continue; }

    if (want_check) md5b.add(buf, n);

    size_t w = Update.write(buf, (size_t)n);
    if (w != (size_t)n) {
      ota_evt("ota_write_err", 0, "Write error");
      Update.abort();
      http.stop();
      return;
    }

    written += w;

    if (total > 0) {
      uint32_t now = millis();
      if ((int32_t)(now - lastMs) >= 150) {
        float pct = (written * 100.0f) / (float)total;
        if (pct < 0) pct = 0; if (pct > 100) pct = 100;
        ota_evt("ota_progress", pct);
        watchdog_feed(); 
        lastMs = now;
      }
    }
    yield();
  }
  http.stop();

  // MD5 verify (if provided)
  if (want_check) {
    md5b.calculate();
    String got = md5b.toString(); // lowercase hex

    auto hexEq = [](const char* a, const char* b){
      for (int i=0;i<32;i++){
        char ca=a[i], cb=b[i];
        if (!ca || !cb) return false;
        if (ca>='A'&&ca<='F') ca = ca - 'A' + 'a';
        if (cb>='A'&&cb<='F') cb = cb - 'A' + 'a';
        if (ca!=cb) return false;
      }
      return true;
    };

    if (!hexEq(got.c_str(), wanted_md5)) {
      mqtt_publish_event("ota_md5_mismatch", 1, 1, "Update", "Update Fail");      
      Update.abort();
      return;
    }
    
  }

  if (!Update.end(true)) {
    ota_evt("ota_end_err", 0, "End failed");
    return;
}

ota_evt("ota_complete", 100.0f, "ota_complete");

// Publish version info (for Grafana dashboard)
mqtt_publish_event("firmware_version", 0, 0, "OTA",
                   (String("FW ") + FW_VERSION_STRING).c_str());

   delay(250);
   if(reboot) ESP.restart(); 
}
