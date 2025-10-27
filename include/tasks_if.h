#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include "web_modbus.h"


// ---------- Queues / Sync ----------
extern QueueHandle_t qModbusCmd;   // app -> modbus writes
extern SemaphoreHandle_t mModbus;  // UART guard

// Commands for Modbus task
enum ModbusCmdType { MB_WRITE_V, MB_WRITE_I, MB_WRITE_STATE };
struct ModbusCmd { ModbusCmdType type; uint8_t id; uint16_t value; };

extern QueueHandle_t qModbusCmd;
extern SemaphoreHandle_t mModbus;

void start_system_tasks();

