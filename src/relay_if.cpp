#include "relay_if.h"
#include <Arduino.h>
#include <ctype.h>
#include "mqtt_if.h"
#include "config.h"
#include "app_settings.h"
#include "debug_log.h"
// --------------------------------------------------------------------
// ✅ NOTE: This module is independent of DPMState and its enum class.
// It works unchanged with the new DPMState::Status type in config.h.
// --------------------------------------------------------------------

// ====== TCA9554 (PCA9554) ======
#define TCA9554_ADDR 0x20
#define REG_INPUT 0x00
#define REG_OUTPUT 0x01
#define REG_POLARITY 0x02
#define REG_CONFIG 0x03

// ===========================================================
// [SECTION Relay] Set Relay Inverted Logic
// ===========================================================

// Local state
static uint8_t g_mask = 0x00;  // bit=1 => ON
static bool g_inverted = true; // Relay inverted logic?

// Small helpers
static bool i2cWriteReg(uint8_t reg, uint8_t val)
{
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission(true) == 0);
}

static inline uint8_t applyInvert(uint8_t mask)
{
  return g_inverted ? (uint8_t)(~mask) : mask;
}

// Public API
void relay_if_set_inverted(bool inverted)
{
  g_inverted = inverted;
  // If you prefer hardware inversion, write REG_POLARITY instead:
  // i2cWriteReg(REG_POLARITY, inverted ? 0xFF : 0x00);
  // For now we keep hardware POL=0 and do it in software.
  i2cWriteReg(REG_OUTPUT, applyInvert(g_mask));
}
// ===========================================================
// [SECTION Relay] Relay init  ON/OFF at Strtup
// ===========================================================

void relay_if_init()
{
  // All pins outputs, no polarity inversion, all off
  i2cWriteReg(REG_CONFIG, 0x00);   // 0=output
  i2cWriteReg(REG_POLARITY, 0x00); // hardware: active-HIGH
  g_mask = 0xff;  // If polarity invertet, all off is 0xFF
  i2cWriteReg(REG_OUTPUT, applyInvert(g_mask)); // Relay Set Output
}

bool relay_if_write_mask(uint8_t mask)
{
  g_mask = mask & 0x00;
  return i2cWriteReg(REG_OUTPUT, applyInvert(g_mask));
}

uint8_t relay_if_read_mask()
{
  return g_mask;
}

bool relay_if_set(uint8_t relay, bool on)
{
  if (relay < 1 || relay > 8)
    return false;
  uint8_t bit = (1u << (relay - 1));
  if (on)
    g_mask |= bit;
  else
    g_mask &= (uint8_t)~bit;
  return i2cWriteReg(REG_OUTPUT, applyInvert(g_mask));
}

bool relay_if_toggle(uint8_t relay)
{
  if (relay < 1 || relay > 8)
    return false;
  g_mask ^= (1u << (relay - 1));
  return i2cWriteReg(REG_OUTPUT, applyInvert(g_mask));
}

// ===== MQTT glue =====
static String topic_relay_set(const String &base, const String &dev, uint8_t n)
{
  return base + "/" + dev + "/relay/" + String(n) + "/set";
}
static String topic_relay_state(const String &base, const String &dev, uint8_t n)
{
  return base + "/" + dev + "/relay/" + String(n);
}
static String topic_relays_set(const String &base, const String &dev)
{
  return base + "/" + dev + "/relays/set";
}

void relay_if_mqtt_subscribe(PubSubClient &mqtt,
                             const String &baseTopic,
                             const String &deviceHost)
{
  for (uint8_t i = 1; i <= 8; i++)
  {
    mqtt.subscribe(topic_relay_set(baseTopic, deviceHost, i).c_str());
  }
  mqtt.subscribe(topic_relays_set(baseTopic, deviceHost).c_str());
}

void relay_if_mqtt_publish_states(PubSubClient &mqtt)
{
  // Not used directly; publishing is done in onConnect path,
  // but keeping the symbol to allow future reuse if needed.
}

// Internals for parsing/publishing
static inline void publish_all(PubSubClient &mqtt,
                               const String &baseTopic,
                               const String &deviceHost)
{
  for (uint8_t i = 1; i <= 8; i++)
  {
    bool on = ((g_mask >> (i - 1)) & 1u) != 0;
    mqtt.publish(topic_relay_state(baseTopic, deviceHost, i).c_str(),
                 on ? "ON" : "OFF", true);
  }
}

static bool parse_boolish(const String &in, bool &out, bool &isToggle)
{
  String s = in;
  s.trim();
  s.toUpperCase();
  if (s == "ON" || s == "1" || s == "TRUE")
  {
    out = true;
    isToggle = false;
    return true;
  }
  if (s == "OFF" || s == "0" || s == "FALSE")
  {
    out = false;
    isToggle = false;
    return true;
  }
  if (s == "TOGGLE" || s == "T")
  {
    isToggle = true;
    return true;
  }
  // tiny JSON support: {"state":"ON"}
  if (s.startsWith("{") && s.indexOf("STATE") >= 0)
  {
    if (s.indexOf("\"ON\"") >= 0)
    {
      out = true;
      isToggle = false;
      return true;
    }
    if (s.indexOf("\"OFF\"") >= 0)
    {
      out = false;
      isToggle = false;
      return true;
    }
  }
  return false;
}

bool relay_if_mqtt_handle(PubSubClient &mqtt,
                          const String &baseTopic,
                          const String &deviceHost,
                          char *topic, byte *payload, unsigned int len)
{
  String t(topic);
  String p;
  p.reserve(len);
  for (unsigned i = 0; i < len; i++)
    p += (char)payload[i];

  // Batch bitmask
  const String T_ALL = topic_relays_set(baseTopic, deviceHost);
  if (t == T_ALL)
  {
    uint32_t val = 0;
    if (p.startsWith("0x") || p.startsWith("0X"))
      val = strtoul(p.c_str(), nullptr, 16);
    else
      val = strtoul(p.c_str(), nullptr, 10);
    relay_if_write_mask((uint8_t)(val & 0xFF));
    publish_all(mqtt, baseTopic, deviceHost);
    return true;
  }

  // Per-channel
  for (uint8_t i = 1; i <= 8; i++)
  {
    const String T_SET = topic_relay_set(baseTopic, deviceHost, i);
    if (t == T_SET)
    {
      bool v = false, isToggle = false;
      if (!parse_boolish(p, v, isToggle))
        return true; // handled (invalid payload)
      bool ok = isToggle ? relay_if_toggle(i) : relay_if_set(i, v);
      (void)ok;
      // --- Publish retained relay state ---
      bool now = ((g_mask >> (i - 1)) & 1u) != 0;
      mqtt.publish(topic_relay_state(baseTopic, deviceHost, i).c_str(),
              now ? "ON" : "OFF", true);

// --- Publish unified event (handled by mqtt_publish_event) ---
        mqtt_publish_event(
            "relay_switched",                       // type
            (i <= ROWS) ? dpms[i].user : 0,         // user id
            i,                                      // DPM number
            now ? "ON" : "OFF",                     // state
            now ? "Relay switched ON" : "Relay switched OFF" // message
        );

        DBG_INFO("[MQTT] Relay %d switched %s (event published)\n",
                 i, now ? "ON" : "OFF");

        return true; // stop after handling this channel
    }
  }

  return false; // not a relay topic
}
