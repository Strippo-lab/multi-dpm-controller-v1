#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ModbusMaster.h>

// ========================= User wiring (edit to your board) =========================

#define PIN_RS485_RX 18 // UART2 RX
#define PIN_RS485_TX 17 // UART2 TX

// ========================= Modbus scan window / table size ==========================
// We no longer hardcode ROWS at compile time.
// ROWS will be set at runtime after the Modbus scan finishes.
#define MAX_DPMS 8               // <<< hard maximum supported devices
extern int ROWS;                 // <<< actual number of detected devices
#define DPMS_SIZE (MAX_DPMS + 1) // backing array size for dpms[], cfg tables

// ========================= DPM state struct (your fields kept) ======================
enum DPMControlMode
{
  MODE_TIME = 0,
  MODE_ENERGY = 1
};

struct DPMState
{
  // ------------------------------------------------------------------
  // State machine states (converted to modern scoped enum class)
  // ------------------------------------------------------------------
  enum class Status : uint8_t
  {
    IDLE = 0,
    INIT,
    WAIT_CURRENT,
    WAIT_REMOVE,
    RUN,
    ADJUST_VOLTAGE,
    CHECK_CONTACT,
    ERROR_STATE,
    DPM_OFF,
    DEFECT,
    TEMP_HIGH, // warning level
    OVERHEAT,  // critical level → DPM forced off
    CHECK_ENERGY
  };

  Status state = Status::IDLE; // FSM (was IDLE before) ✅

  int dpm_state = 0; // ON/OFF
  int volt_act = 0;  // actual voltage (raw)
  int cur_act = 0;   // actual current (raw)
  int temp_act = 0;  // actual temperature
  long unsigned int remain_time = 0;
  int volt_set = 300;
  int cur_set = 300;
  int idle_cur = 300;
  int start_volt = 300;
  long unsigned int last_ms = 0; // last update timestamp
  long unsigned int runtime = 0;
  long unsigned int waitTimer = 0;
  int percent = 0;
  int reserved2 = 0;
  int reserved3 = 0;
  uint8_t id;            // real Modbus slave ID
  bool valid = false;    // true once we’ve seen this ID
  uint8_t error_cnt = 0; // <--- NEW: consecutive errors
  uint8_t curve_mode = 0; // 0 = Normal, 1 = Ramp, 2 = Pulse
  // --- ENERGY CONTROL ---
  DPMControlMode mode = MODE_TIME;
  double energy_temp = 0.0;   // batch
  double energy_total = 0.0;  // lifetime
  double energy_anode = 0.0;  // since anode change
  double energy_target = 0.0; // expected (for energy mode)
  unsigned long last_energy_ms = 0;
  double anode_threshold = 100000.0; // J threshold for service alert
  // --- OPERATOR INFO ---
  int user = 888;  // last known operator number
  int line_id = 0;  // NEW: Galvanic line identifier (0 = none, 1 = Line1, 2 = Line2 ...)
  // --- CURRENT CURVE CONTROL ---
  struct CurveControl
  {
    bool active = false;            // Curve enabled
    uint8_t type = 0;               // 0=none, 1=linear ramp, 2=pulse
    unsigned long start_ms = 0;     // start time
    unsigned long duration = 0;     // for ramp (ms)
    int start_cur = 0;              // ramp start current (mA)
    int end_cur = 0;                // ramp end current (mA)
    int pulse_high = 0;             // ON current (mA)
    int pulse_low = 0;              // OFF current (mA)
    unsigned long pulse_period = 0; // full period (ms)
  } curve;
};

// App config
struct Config
{
  uint32_t lastStatusMs = 0;
  uint32_t statusPeriodMs = 10000;
};

// ========================= Globals (declared here, defined in globals.cpp) ==========
extern Config C;
extern DPMState dpms[DPMS_SIZE]; // index == Modbus ID (1..ROWS)

// Per-ID scan results (used by HTTP UI and reader)
extern int8_t g_cfgForId[DPMS_SIZE];       // -1 unknown, else cfg index
extern uint32_t g_cfgMaskForId[DPMS_SIZE]; // bitmask of all cfgs that answered
extern uint8_t g_foundIds[DPMS_SIZE];
extern int g_foundCount;

// UART & Modbus master (IMPORTANT: extern here, defined once in globals.cpp)
// Default UART settings used when we first bring up the bus

constexpr uint32_t MODBUS_DEFAULT_CFG = SERIAL_8N1; // 8N1 by default

extern HardwareSerial modbus;
extern ModbusMaster DPM_1;
//******************************************************** Variables *************************************************************************************** */
#define DPM_TEMP_WARN 45 //
#define DPM_TEMP_CRIT 50 //

// ========================= Public functions =========================================

// RS485 driver control (implemented in modbus_scan.cpp)

// HTTP server (implemented in web_modbus.cpp)
void http_begin(); // register handlers and start server

// Helpers to show cfg names in UI (provided by modbus_scan.cpp)

void handle_StateMachine();
void loadConfig();
void saveConfig();
void printConfig();
uint8_t dpm_write_current(uint8_t nr, uint16_t c);
uint8_t dpm_write_voltage(uint8_t nr, uint16_t v);
uint8_t dpm_write_state(uint8_t nr, bool s);
