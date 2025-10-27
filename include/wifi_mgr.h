#pragma once
#include <Arduino.h>

// ---- Tunables (override in platformio.ini via -DNAME=value if you want) ----
#ifndef WIFI_BACKOFF_MIN_MS
#define WIFI_BACKOFF_MIN_MS  500u
#endif
#ifndef WIFI_BACKOFF_MAX_MS
#define WIFI_BACKOFF_MAX_MS  60000u
#endif
#ifndef WIFI_ROAM_CHECK_MS
#define WIFI_ROAM_CHECK_MS   30000u   // scan while connected this often
#endif
#ifndef WIFI_ROAM_RSSI_TRIGGER
#define WIFI_ROAM_RSSI_TRIGGER  -70   // if RSSI worse than this, consider roaming
#endif
#ifndef WIFI_ROAM_MARGIN_DB
#define WIFI_ROAM_MARGIN_DB     8     // need at least this many dB better to roam
#endif
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 15000u
#endif

// Optional: set a hostname (override in build flags if you like)
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "DPM"
#endif
static volatile uint32_t g_wifiUpSince = 0;
static volatile bool     g_scanActive  = false;   // set this from wifi_mgr if you run scans
// API
void wifi_setup();          // call once in setup()
void wifi_loop();           // call every loop()
bool wifi_connected();      // current link state
int  wifi_rssi();           // current RSSI (or INT_MIN if unknown)
String wifi_ip();           // dotted IP when connected

// Manual actions (rarely needed)
void wifi_force_reconnect();  // drop + reconnect now
void wifi_request_roam();     // scan now and roam if better AP exists


