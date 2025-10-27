#pragma once
// ---- Network & MQTT ----
// Edit these for your environment
namespace App {
  inline constexpr const char* WIFI_SSID  = "FAMAU7530";
  inline constexpr const char* WIFI_PASS  = "78227180465236262058";

  inline constexpr const char* MQTT_HOST  = "192.168.178.43";
  inline constexpr uint16_t    MQTT_PORT  = 1883;
  inline constexpr const char* MQTT_USER  = "";   // optional
  inline constexpr const char* MQTT_PASS  = "";   // optional

  // Base MQTT topic for this device
  inline constexpr const char* BASE_TOPIC = "DPM_Control";
  inline constexpr const char* DEVICE_HOST = "TEST_DPM";  
  static constexpr const char* CLUSTER_NAME  = "Cluster01";
}

