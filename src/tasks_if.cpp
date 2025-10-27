#include "tasks_if.h"
#include "config.h"
#include "modbus_if.h"
#include "mqtt_if.h"
#include "eth_mgr.h"
#include "watchdog.h"

// --------------------------------------------------------------------
// ✅ NOTE: This file does not reference DPMState::Status directly.
// It remains fully compatible with the new enum class version.
// --------------------------------------------------------------------

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Periods (tune as you like)
// -------------------------------------------------------------------
#define HTTP_PERIOD_MS        50
#define ETH_PERIOD_MS        100
#define MODBUS_PERIOD_MS     200
#define WDT_PERIOD_MS       1000
#define STATE_PERIOD_MS      100   // e.g. 50–200 ms
#define INFLUX_PERIOD_MS    5000   // 1 sample / 5s
#define DPM_STATUS_PERIOD_MS 1000  // 1 sample / 1s

// -------------------------------------------------------------------
// Externs (from other modules)
// -------------------------------------------------------------------
extern HardwareSerial modbus;
extern ModbusMaster   DPM_1;

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Globals
// -------------------------------------------------------------------
QueueHandle_t qModbusCmd;
SemaphoreHandle_t mModbus;

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] HTTP task
// -------------------------------------------------------------------
static void httpTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    http_poll();
    watchdog_feed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(HTTP_PERIOD_MS));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Ethernet task
// -------------------------------------------------------------------
static void ethTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    eth_loop();
    watchdog_feed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(ETH_PERIOD_MS));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Status publisher task (enqueue request every 1s)
// -------------------------------------------------------------------
static void statusTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    mqtt_request_status(true);   // enqueue status publish
    vTaskDelayUntil(&last, pdMS_TO_TICKS(DPM_STATUS_PERIOD_MS));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Influx publisher task (enqueue request every 5s)
// -------------------------------------------------------------------
static void influxTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    mqtt_request_influx();       // enqueue influx publish
    vTaskDelayUntil(&last, pdMS_TO_TICKS(INFLUX_PERIOD_MS));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Modbus task
// -------------------------------------------------------------------
static void modbusTask(void*) {
  init_modbus_async_begin();      // non-blocking scanner

  TickType_t lastRead = xTaskGetTickCount();

  for (;;) {
    // 1) Finish incremental scan first
    if (!modbus_scan_finished()) {
      if (xSemaphoreTake(mModbus, pdMS_TO_TICKS(50)) == pdTRUE) {
        modbus_scan_step();       // short step (has tiny delays/yield)
        xSemaphoreGive(mModbus);
      }
      watchdog_feed();
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    // 2) Handle queued writes
    ModbusCmd cmd;
    while (xQueueReceive(qModbusCmd, &cmd, 0) == pdTRUE) {
      if (xSemaphoreTake(mModbus, pdMS_TO_TICKS(200)) == pdTRUE) {
        switch (cmd.type) {
          case MB_WRITE_V: DPM_1.begin(cmd.id, modbus); DPM_1.writeSingleRegister(0, cmd.value); break;
          case MB_WRITE_I: DPM_1.begin(cmd.id, modbus); DPM_1.writeSingleRegister(1, cmd.value); break;
          case MB_WRITE_STATE: DPM_1.begin(cmd.id, modbus); DPM_1.writeSingleRegister(2, cmd.value); break;
        }
        xSemaphoreGive(mModbus);
      }
      taskYIELD();
    }

    // 3) Periodic read of all found devices
    if (xTaskGetTickCount() - lastRead >= pdMS_TO_TICKS(MODBUS_PERIOD_MS)) {
      if (xSemaphoreTake(mModbus, pdMS_TO_TICKS(200)) == pdTRUE) {
        readModbus();             // updates dpms[]
        xSemaphoreGive(mModbus);
      }
      lastRead = xTaskGetTickCount();
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] State machine task
// -------------------------------------------------------------------
static void stateTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    handle_StateMachine();       // FSM from config.h
    watchdog_feed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(STATE_PERIOD_MS));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Watchdog feed task
// -------------------------------------------------------------------
static void watchdogTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    watchdog_feed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(WDT_PERIOD_MS));
  }
}

// -------------------------------------------------------------------
// [SECTION TASK FREERTOS] Start all system tasks
// -------------------------------------------------------------------
void start_system_tasks() {
  qModbusCmd = xQueueCreate(16, sizeof(ModbusCmd));
  mModbus    = xSemaphoreCreateMutex();
// ✅ Create publish queue early
  qMqttPublish = xQueueCreate(32, sizeof(MqttMsg));
  xTaskCreatePinnedToCore(httpTask,     "httpTask",     4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(ethTask,      "ethTask",      3072, nullptr, 2, nullptr, 0);

  // NEW: centralized MQTT task (defined in mqtt_if.cpp)
  start_mqtt_task();

  xTaskCreatePinnedToCore(statusTask,   "statusTask",   4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(modbusTask,   "modbusTask",   6144, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(stateTask,    "stateTask",    4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(watchdogTask, "watchdogTask", 2048, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(influxTask,   "influxTask",   4096, nullptr, 2, nullptr, 1);
}
