#include "eth_mgr.h"
#include <SPI.h>
#include <Ethernet.h>   // Arduino Ethernet (W5500)
#include <esp_mac.h>    // esp_read_mac()
#include "net_client.h"
#include "debug_log.h"

static EthernetClient ethClient;


// ----- Waveshare ESP32-S3-ETH W5500 pins -----
static constexpr int PIN_SCK  = 15;
static constexpr int PIN_MISO = 14;
static constexpr int PIN_MOSI = 13;
static constexpr int PIN_CS   = 16;
static constexpr int PIN_IRQ  = 12;     // not used by Ethernet.h
static constexpr int PIN_RST  = -1;     // set to -1 if not wired

// ----- Internals -----
static EthernetClient g_eth;
static bool g_started = false;
static bool g_use_static = false;
static IPAddress g_ip, g_gw, g_mask, g_dns;
static unsigned long g_next_dhcp_try = 0;
static EthernetLinkStatus g_last_link = Unknown;


Client& eth_client() { return g_eth; }

static void dhcp_start_or_static() {
  uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_ETH);

  int ok = Ethernet.begin(mac);          // DHCP
  if (ok == 0 && g_use_static && g_ip != IPAddress(0,0,0,0)) {
    // static fallback
    IPAddress dns = (g_dns != IPAddress(0,0,0,0)) ? g_dns : g_gw;
    Ethernet.begin(mac, g_ip, dns, g_gw, g_mask);
  }
}

bool eth_setup(bool staticFallback,
               IPAddress ip, IPAddress gw, IPAddress mask, IPAddress dns)
{
  g_use_static = staticFallback;
  g_ip   = ip;
  g_gw   = gw;
  g_mask = mask;
  g_dns  = dns;

  // SPI + CS + optional reset
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  Ethernet.init(PIN_CS);
  

  dhcp_start_or_static();

  g_started = true;
  g_last_link = Ethernet.linkStatus();

  DBG_INFO("[ETH] IP=");   Serial.println(Ethernet.localIP());
  DBG_INFO("[ETH] LINK="); Serial.println(g_last_link == LinkON ? "UP" : "DOWN");

  return Ethernet.localIP() != IPAddress(0,0,0,0);
  gNetClient = &ethClient;
}

void eth_loop() {
  if (!g_started) return;

  // Keep DHCP lease fresh
  Ethernet.maintain();

  // Detect link changes and (re)start DHCP if needed
  EthernetLinkStatus st = Ethernet.linkStatus();
  if (st != g_last_link) {
    g_last_link = st;
    DBG_INFO("[ETH] 🌐 Link %s\n", (st == LinkON ? "✅ UP" : "❌ DOWN"));
    if (st == LinkON && Ethernet.localIP() == IPAddress(0,0,0,0)) {
      // link came back but no IP -> retry DHCP (rate-limited)
      unsigned long now = millis();
      if (now >= g_next_dhcp_try) {
        dhcp_start_or_static();
        g_next_dhcp_try = now + 3000;
      }
    }
  }
}

bool eth_link_up() {
  return Ethernet.linkStatus() == LinkON;
}

IPAddress eth_ip() {
  return Ethernet.localIP();
}
