#include <Arduino.h>
#include "app_settings.h"
#include "pins.h"
#include "config.h"
#include "mqtt_if.h"
#include "watchdog.h"
#include "wifi_mgr.h" // declares wifi_setup(), wifi_loop(), wifi_connected()
#include "statemachine_mgr.h"
#include "eth_mgr.h"
#include <WiFi.h>
#include "relay_if.h"
#include "modbus_if.h"
#include "tasks_if.h"
#include "debug_log.h"
// RS485 on UART1 (pins from your config)
#define TXD1 17
#define RXD1 18
#define RS485_SERIAL Serial1

// >>>>>>>>>>>> EDIT YOUR WIFI SETTINGS <<<<<<<<<<<<<<
static const char *WIFI_SSID = "DPM-Scanner";
static const char *WIFI_PASS = "12345678"; // for SoftAP
unsigned long modbus_timer = 0;

// -------------------------------------------------------------------
// System-init task: waits for Modbus scan to finish, then starts system
// -------------------------------------------------------------------
void task_system_start(void *pv)
{
    DBG_INFO("[SYS] Waiting for Modbus scan to finish...\n");

    while (!g_modbusScanDone) {
        vTaskDelay(pdMS_TO_TICKS(500));   // sleep 0.5 s
    }

    DBG_INFO("[SYS] Modbus scan done, starting system init...\n");

    loadConfig();
    printConfig();
    http_begin();
    mqtt_init();
    relay_if_init();  
    initStatemachine();

    DBG_INFO("[SYS] ✅ System initialization complete\n");

    vTaskDelete(NULL);  // one-shot task → self-delete
}


void setup()
{
  RS485_SERIAL.begin(9600, SERIAL_8N1, RXD1, TXD1);
  Serial.begin(115200);
  delay(150);
  watchdog_start(3 /*seconds*/);   // e.g. 3s global timeout
  if (!eth_setup())
  {
    DBG_ERROR("[ETH] ❌ DHCP failed (no static fallback)");
  }
  // Bring up WiFi as AP (or switch to STA if you prefer)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  DBG_INFO("[WiFi] ✅ AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  Wire.begin(42, 41, 100000);// I2C relay setting
  start_system_tasks(); // start FreeRTOS tasks
  // wifi_setup();
  init_modbus_async_begin(); 
  // 🔹 Launch the waiting/init task
    xTaskCreatePinnedToCore(
        task_system_start,         // function
        "SysInit",                 // name
        4096,                      // stack size
        NULL,                      // param
        1,                         // priority
        NULL,                      // handle (unused)
        1                          // core (1 = APP core)
    );
}

void loop()
{

vTaskDelay(pdMS_TO_TICKS(1000));  

}
