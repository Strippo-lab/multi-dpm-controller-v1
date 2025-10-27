#include "config.h"

// ========================= Global instances =========================

// --------------------------------------------------------------------
// Application-wide configuration/state
// --------------------------------------------------------------------
// Holds generic timing and settings that are not tied to a single DPM.
// For example, MQTT status publishing uses C.lastStatusMs to know when
// the last update was sent. Central place for “soft settings”.
// --------------------------------------------------------------------
Config C{};

// --------------------------------------------------------------------
// Runtime number of detected DPM devices
// --------------------------------------------------------------------
// ROWS is updated by the Modbus scanner (modbus_scan.cpp) once it
// finishes probing the bus. It reflects how many devices are active
// and should be used in loops everywhere (state machine, MQTT publish,
// etc.). This replaces the old compile-time #define ROWS.
// --------------------------------------------------------------------
int ROWS = 0;   // valid range: 0..MAX_DPMS

// --------------------------------------------------------------------
// Array of DPM states
// --------------------------------------------------------------------
// Each index (0..ROWS-1) corresponds to one DPM device, with Modbus ID
// = index+1. This array holds the current FSM state, measurements and
// settings for each device.
// --------------------------------------------------------------------
// ⚙️ DPMState now uses enum class Status : uint8_t for type-safe state handling
DPMState dpms[DPMS_SIZE]{};   // backing array, max size = MAX_DPMS

// --------------------------------------------------------------------
// Modbus scan results
// --------------------------------------------------------------------
// Tables used by the async scanner to remember which serial configs
// worked for which IDs, and which devices responded. This is used to
// build the final ROWS count and to decide which baud/parity config
// to use when talking to each device.
// --------------------------------------------------------------------
int8_t   g_cfgForId[DPMS_SIZE]     = {0};
uint32_t g_cfgMaskForId[DPMS_SIZE] = {0};
uint8_t  g_foundIds[DPMS_SIZE]     = {0};
int      g_foundCount              = 0;

// --------------------------------------------------------------------
// Hardware handles
// --------------------------------------------------------------------
// Global UART2 object and ModbusMaster instance. These are shared by
// all Modbus I/O and protected by a mutex in FreeRTOS tasks. Only one
// transaction should be active at a time.
// --------------------------------------------------------------------
HardwareSerial modbus(2);
ModbusMaster   DPM_1;
