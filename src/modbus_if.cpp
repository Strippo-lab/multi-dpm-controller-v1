#include "config.h"
#include "pins.h"
#include "tasks_if.h"
#include <Arduino.h>
#include <ModbusMaster.h>
#include "statemachine_mgr.h"
#include "mqtt_if.h"
#include "debug_log.h"

// =====================================================================
// External globals (declared once in globals.cpp, shared everywhere)
// =====================================================================
extern HardwareSerial modbus; // UART2 instance for RS485
extern ModbusMaster DPM_1;    // Modbus master bound to UART2
extern DPMState dpms[DPMS_SIZE];

extern int8_t g_cfgForId[DPMS_SIZE];
extern uint32_t g_cfgMaskForId[DPMS_SIZE];
extern uint8_t g_foundIds[DPMS_SIZE];
extern int g_foundCount;

// =====================================================================
// Serial configurations to try during scan (baud + framing)
// Order matters → will scan each config in sequence
// =====================================================================
struct SerialCfg
{
  uint32_t baud;
  uint32_t config;
  const char *name;
};
static const SerialCfg SERIAL_CFGS[] = {
    {57600, SERIAL_8N1, "57600 8N1"},
    {9600, SERIAL_8N1, "9600 8N1"},
    {19200, SERIAL_8N1, "19200 8N1"},
    {38400, SERIAL_8N1, "38400 8N1"},
    {115200, SERIAL_8N1, "115200 8N1"},
};
static const int SERIAL_CFG_COUNT = sizeof(SERIAL_CFGS) / sizeof(SERIAL_CFGS[0]);

// =====================================================================
// Modbus register map & timing
// =====================================================================
static const uint16_t REQ_TIMEOUT_MS = 150; // Short timeout → keep watchdog happy
static const uint8_t RETRIES = 1;           // How many retries per ID per cfg
static const uint16_t PING_ADDR = 0x1000;   // DPM8624: status register start
static const uint16_t PING_LEN = 1;         // Minimal read for presence check
static const uint16_t READ_ADDR = 0x1000;   // Start of full block read
static const uint16_t READ_LEN = 8;         // Length of block read (status/values)

// =====================================================================
// NOTE: preTransmission() and postTransmission() were removed
// → Your RS485 transceiver auto-handles DE/RE direction
// → No manual GPIO toggle required here
// =====================================================================

// =====================================================================
// Helper: apply a given UART configuration to the Modbus serial
// =====================================================================
static inline void applySerialCfg(const SerialCfg &c)
{
  modbus.end(); // close if already open
  modbus.begin(c.baud, c.config, PIN_RS485_RX, PIN_RS485_TX);
  modbus.setTimeout(REQ_TIMEOUT_MS);
}

// =====================================================================
// Helper: test if a given Modbus ID responds with current config
// =====================================================================
static bool pingId(uint8_t id)
{
  DPM_1.begin(id, modbus);
  return (DPM_1.readHoldingRegisters(PING_ADDR, PING_LEN) == DPM_1.ku8MBSuccess);
}

// =====================================================================
// Incremental scan state machine (non-blocking)
// =====================================================================
static int scan_cfgIdx = 0;    // which serial config we’re testing
static uint8_t scan_id = 1;    // current Modbus ID being tested
static bool scan_done = false; // set true once finished
bool g_modbusScanDone = false;   // global state flag
void init_modbus_async_begin()
{
  // Reset tables
  for (int i = 0; i < DPMS_SIZE; ++i)
  {
    g_cfgForId[i] = -1;
    g_cfgMaskForId[i] = 0;
    dpms[i].valid = false;
  }
  g_foundCount = 0;

  scan_cfgIdx = 0;
  scan_id = 1; // always start at ID=1
  scan_done = false;

  // Apply first config and start scan
  applySerialCfg(SERIAL_CFGS[scan_cfgIdx]);
  DBG_INFO("[SCAN] start cfg=%d (%s)\n", scan_cfgIdx, SERIAL_CFGS[scan_cfgIdx].name);
}

// =====================================================================
// Perform a single scan step → call repeatedly until scan_done=true
// Returns true if finished, false if still scanning
// =====================================================================
bool modbus_scan_step()
{
  if (scan_done)
    return true;

  const auto &cfg = SERIAL_CFGS[scan_cfgIdx];
  DBG_INFO("[SCAN] cfg=%d (%s), id=%u ... ", scan_cfgIdx, cfg.name, scan_id);

  if (g_cfgForId[scan_id] == -1)
  {
    bool ok = false;
    for (uint8_t r = 0; r <= RETRIES && !ok; ++r)
    {
      ok = pingId(scan_id);
      delay(1);
      yield(); // feed watchdog
    }

    if (ok)
    {
      g_cfgMaskForId[scan_id] |= (1u << scan_cfgIdx);
      g_cfgForId[scan_id] = scan_cfgIdx;
      dpms[scan_id].valid = true;
      if (g_foundCount < DPMS_SIZE - 1)
        g_foundIds[g_foundCount++] = scan_id;
      DBG_INFO("FOUND ✅\n");
    }
    else
    {
       DBG_ERROR("❌ no response\n");
    }
  }
  else
  {
     DBG_INFO("already solved ✅\n");
  }

  // Advance to next ID/config
  scan_id++;
  if (scan_id > MAX_DPMS)
  {
    scan_cfgIdx++;
    if (scan_cfgIdx >= SERIAL_CFG_COUNT)
    {
      scan_done = true;
      ROWS = g_foundCount; // <--- update detected count
      g_modbusScanDone = true;    // ✅ mark scan completed     

      DBG_INFO("[SCAN] finished ✅");
      return true;
    }
    scan_id = 1;
    applySerialCfg(SERIAL_CFGS[scan_cfgIdx]);
    DBG_INFO("[SCAN] switch cfg=%d (%s)\n", scan_cfgIdx, SERIAL_CFGS[scan_cfgIdx].name);
  }
  return false;
}

bool modbus_scan_finished() { return scan_done; }

// =====================================================================
// Poll all detected devices using their working config
// Updates dpms[id] with fresh values
// =====================================================================
#define MAX_ERR 5 // consecutive failed reads before declaring DEFECT

void readModbus()
{
  for (int i = 0; i < g_foundCount; ++i)
  {
    uint8_t id = g_foundIds[i]; // real Modbus slave ID
    int8_t cfgIx = g_cfgForId[id];
    if (cfgIx < 0)
      continue;

    // --- Skip devices explicitly OFF or already DEFECT ---
    if (dpms[id].state == DPMState::Status::DPM_OFF || // ✅ updated
        dpms[id].state == DPMState::Status::DEFECT)    // ✅ updated
    {
      dpms[id].valid = false;
      continue;
    }

    applySerialCfg(SERIAL_CFGS[cfgIx]);
    delay(1);

    DPM_1.begin(id, modbus);
    uint8_t rc = DPM_1.readHoldingRegisters(READ_ADDR, READ_LEN);
    if (rc == DPM_1.ku8MBSuccess)
    {
      // ✅ Successful read → update values
      dpms[id].dpm_state = DPM_1.getResponseBuffer(0);
      dpms[id].volt_act = DPM_1.getResponseBuffer(1);
      dpms[id].cur_act = DPM_1.getResponseBuffer(2);
      dpms[id].temp_act = DPM_1.getResponseBuffer(3);
      dpms[id].valid = true;

      // Reset error counter, recover from DEFECT if needed
      dpms[id].error_cnt = 0;

      // --- State checks ---
      // --- Temperature logic ---
      if (dpms[id].temp_act >= DPM_TEMP_CRIT)
      {
        // Critical → force OFF and mark OVERHEAT
        dpms[id].state = DPMState::Status::OVERHEAT; // ✅ updated
      }
      else if (dpms[id].temp_act >= DPM_TEMP_WARN)
      {
        // Warning only
        dpms[id].state = DPMState::Status::TEMP_HIGH; // ✅ updated
      }
      else
      {
        // Recover from TEMP_HIGH/OVERHEAT/DEFECT back to RUN
        if (dpms[id].state == DPMState::Status::TEMP_HIGH) // ✅ updated
        {
          dpms[id].state = DPMState::Status::RUN; // ✅ updated
        }
      }
    }
    else
    {
      // ❌ Failed read → increment error counter
      dpms[id].error_cnt++;
      if (dpms[id].error_cnt >= MAX_ERR)
      {
        dpms[id].state = DPMState::Status::DEFECT; // ✅ updated
        dpms[id].valid = false;
      }
    }
    delay(2);
    yield();
  }
}
// =====================================================================
// Human-readable serial cfg info (for UI/debug)
// =====================================================================
const char *modbus_cfg_name(int idx)
{
  if (idx < 0 || idx >= SERIAL_CFG_COUNT)
    return "unknown";
  return SERIAL_CFGS[idx].name;
}
int modbus_cfg_count() { return SERIAL_CFG_COUNT; }

// =====================================================================
// Write helpers → enqueue Modbus commands via FreeRTOS queue
// Called by higher layers (MQTT, state machine, etc.)
// =====================================================================
uint8_t dpm_write_voltage(uint8_t nr, uint16_t v)
{
  ModbusCmd msg{MB_WRITE_V, static_cast<uint8_t>(nr), v};
  return xQueueSend(qModbusCmd, &msg, 0) == pdTRUE ? 0 : 1;
}

uint8_t dpm_write_current(uint8_t nr, uint16_t cur)
{
  ModbusCmd msg{MB_WRITE_I, static_cast<uint8_t>(nr), cur};
  return xQueueSend(qModbusCmd, &msg, 0) == pdTRUE ? 0 : 1;
}

uint8_t dpm_write_state(uint8_t nr, bool s)
{
  ModbusCmd msg{MB_WRITE_STATE, static_cast<uint8_t>(nr), static_cast<uint16_t>(s ? 1 : 0)};
  return xQueueSend(qModbusCmd, &msg, 0) == pdTRUE ? 0 : 1;
}
