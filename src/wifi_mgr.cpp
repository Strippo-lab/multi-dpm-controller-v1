#include "wifi_mgr.h"
#include <WiFi.h>
#include <esp_system.h>
#include "app_settings.h"
#include <cstring>
#include "mqtt_if.h"   // <-- add this at the top of wifi_mgr.cpp
#include "debug_log.h"
// ---- Tunables (can override via -D in platformio.ini) ----
#ifndef WIFI_ROAM_CHECK_MS
#define WIFI_ROAM_CHECK_MS 30000u
#endif
#ifndef WIFI_ROAM_RSSI_TRIGGER
#define WIFI_ROAM_RSSI_TRIGGER -60
#endif
#ifndef WIFI_ROAM_MARGIN_DB
#define WIFI_ROAM_MARGIN_DB 8
#endif
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 15000u
#endif

// ---------------- Internal state ----------------
static bool     s_connected     = false;
static bool     s_connecting    = false;  // avoid double-begin
static uint32_t s_nextAttemptMs = 0;
static uint8_t  s_currBssid[6]  = {0};
static int32_t  s_currChannel   = 0;

// ---- async scan state ----
static bool     s_scanInProgress = false;
static bool     s_haveBest       = false;
static uint32_t s_scanStartedMs  = 0;
static uint8_t  s_bestBssid[6]   = {0};
static int32_t  s_bestChannel    = 0;
static int      s_bestRssi       = -127;
static uint32_t s_bootScanAt = 0;   // when to kick the first async scan
// ---- weak hooks ----
extern "C" void onWifiUp()   { mqtt_try_connect_with_backoff(true); }
extern "C" void onWifiDown() { mqtt_disconnect(); }
// ---------------- Helpers ----------------
static void start_async_scan() {
  if (s_scanInProgress) return;
#ifdef DEBUG_LOG
  Serial.println("[WiFi] Async scan start...");
#endif
  WiFi.scanDelete();
  WiFi.scanNetworks(true);   // ASYNC (returns immediately)
  s_scanInProgress = true;
  s_scanStartedMs  = millis();
  s_haveBest       = false;
  s_bestRssi       = -127;
}

static void process_scan_results() {
  if (!s_scanInProgress) return;
  int res = WiFi.scanComplete();   // -1 scanning, -2 failed, >=0 done
  if (res == -1) {
    // safety timeout
    if ((int32_t)(millis() - s_scanStartedMs) > (int32_t)(WIFI_CONNECT_TIMEOUT_MS * 2)) {
#ifdef DEBUG_LOG
      Serial.println("[WiFi] Scan timeout; cancel.");
#endif
      WiFi.scanDelete();
      s_scanInProgress = false;
    }
    return;
  }
  s_scanInProgress = false;
  if (res < 0) {
#ifdef DEBUG_LOG
    Serial.println("[WiFi] Scan failed.");
#endif
    return;
  }

  int bestIdx = -1;
  int bestRssi = -127;
  for (int i = 0; i < res; ++i) {
    if (WiFi.SSID(i) == App::WIFI_SSID) {
      int r = WiFi.RSSI(i);
      if (r > bestRssi) { bestRssi = r; bestIdx = i; }
    }
  }
  if (bestIdx < 0) {
#ifdef DEBUG_LOG
    Serial.println("[WiFi] SSID not found in scan.");
#endif
    return;
  }

  std::memcpy(s_bestBssid, WiFi.BSSID(bestIdx), 6);
  s_bestChannel = WiFi.channel(bestIdx);
  s_bestRssi    = bestRssi;
  s_haveBest    = true;

#ifdef DEBUG_LOG
  Serial.printf("[WiFi] Best from scan %02X:%02X:%02X:%02X:%02X:%02X ch %d RSSI %d\n",
    s_bestBssid[0], s_bestBssid[1], s_bestBssid[2],
    s_bestBssid[3], s_bestBssid[4], s_bestBssid[5],
    (int)s_bestChannel, s_bestRssi);
#endif
}

static bool bssid_differs(const uint8_t a[6], const uint8_t b[6]) {
  return std::memcmp(a, b, 6) != 0;
}

// ---------------- Event handler ----------------
static void onEvent(WiFiEvent_t ev, WiFiEventInfo_t info) {
#ifdef DEBUG_LOG
  Serial.printf("[WiFi] Event %d\n", ev);
#endif
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
      std::memcpy(s_currBssid, info.wifi_sta_connected.bssid, 6);
      s_currChannel = info.wifi_sta_connected.channel;
      s_connecting  = false;
#ifdef DEBUG_LOG
      Serial.printf("[WiFi] Connected to %02X:%02X:%02X:%02X:%02X:%02X ch %d\n",
        s_currBssid[0],s_currBssid[1],s_currBssid[2],s_currBssid[3],s_currBssid[4],s_currBssid[5],
        (int)s_currChannel);
#endif
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
      s_connected  = true;
      s_connecting = false;
      // prefer to scan soon after we’re online (radio is idle)
    if (s_bootScanAt == 0 || (int32_t)(s_bootScanAt - millis()) > 2000)
    s_bootScanAt = millis() + 2000;    // first scan ~2s after IP
#ifdef DEBUG_LOG
      Serial.printf("[WiFi] Got IP: %s RSSI=%d\n",
        WiFi.localIP().toString().c_str(), WiFi.RSSI());
#endif
      onWifiUp();
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      if (s_connected) onWifiDown();
      s_connected  = false;
      s_connecting = false;
      // Kick a fresh async scan so next connect prefers the best BSSID.
      start_async_scan();
      break;
    }
    default: break;
  }
}

// ---------------- Public API ----------------
void wifi_setup() {
  randomSeed(esp_random());
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);         // scan all channels during connect
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);     // prefer strongest BSSID for the SSID
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);      // retry last AP
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.onEvent(onEvent);

  // If your core supports it, prefer AP by signal (harmless if not present)
  #ifdef WIFI_CONNECT_AP_BY_SIGNAL
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  #endif

#ifdef DEBUG_LOG
  Serial.printf("[WiFi] Connecting to %s ...\n", App::WIFI_SSID);
#endif

  // Fast connect now (no blocking scan), but kick off an async scan in parallel.
  s_connecting = true;
  WiFi.begin(App::WIFI_SSID, App::WIFI_PASS);
  start_async_scan();   // will deliver a best-BSSID candidate shortly
}

bool wifi_connected() { return s_connected && WiFi.status() == WL_CONNECTED; }
int  wifi_rssi()      { return wifi_connected() ? WiFi.RSSI() : INT_MIN; }
String wifi_ip()      { return wifi_connected() ? WiFi.localIP().toString() : String(); }

void wifi_force_reconnect() {
#ifdef DEBUG_LOG
  Serial.println("[WiFi] Force reconnect");
#endif
  s_connected  = false;
  s_connecting = false;
  WiFi.disconnect(false, false);
  start_async_scan();
}

void wifi_request_roam() {
  // Force a new scan; loop will steer if better AP appears.
  start_async_scan();
}

// ---------------- Loop ----------------
void wifi_loop() {

  // 1) Post-boot first scan (only after we’re connected)
if (wifi_connected() && s_bootScanAt && (int32_t)(millis() - s_bootScanAt) >= 0) {
  if (!s_scanInProgress) start_async_scan();
  s_bootScanAt = 0; // run once
}
  
    // Always process async scan completion first
  process_scan_results();

  // If we are DISCONNECTED and have a best candidate, prefer it
  if (!wifi_connected() && !s_connecting && s_haveBest) {
#ifdef DEBUG_LOG
    Serial.printf("[WiFi] Begin toward best BSSID ch %d RSSI %d\n", (int)s_bestChannel, s_bestRssi);
#endif
    s_connecting = true;
    WiFi.begin(App::WIFI_SSID, App::WIFI_PASS, s_bestChannel, s_bestBssid, true);
    s_bootScanAt = millis() + 5000;     // 5s after boot; adjusted in GOT_IP below
    // Clear candidate to avoid flip-flop
    s_haveBest = false;
    return;
  }

#if WIFI_ROAM_CHECK_MS > 0
static uint32_t lastRoamCheck = 0;
uint32_t now = millis();
if (wifi_connected() && (int32_t)(now - lastRoamCheck) >= 0) {
  lastRoamCheck = now + WIFI_ROAM_CHECK_MS;
mqtt_publish_event("resync", 2.2f); // keepalive
  int curr = WiFi.RSSI();
  if (s_haveBest) {
    bool different   = std::memcmp(s_bestBssid, s_currBssid, 6) != 0;
    bool muchBetter  = (s_bestRssi >= curr + WIFI_ROAM_MARGIN_DB);
    if (different && muchBetter && !s_connecting) {
#ifdef DEBUG_LOG
      Serial.printf("[WiFi] Roaming: %d -> %d dBm (switch BSSID)\n", curr, s_bestRssi);
#endif
      s_connecting = true;
      WiFi.begin(App::WIFI_SSID, App::WIFI_PASS, s_bestChannel, s_bestBssid, true);
      s_haveBest = false; // consume
      return;
    }
  }
  // no candidate or not much better? refresh candidate with another async scan
  if (!s_scanInProgress) start_async_scan();
}
#endif
}
