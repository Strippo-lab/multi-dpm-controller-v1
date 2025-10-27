#include "config.h"
#include "statemachine_mgr.h"
#include <Arduino.h>
#include "mqtt_if.h"
#include "debug_log.h"

// =====================================================================
// [SECTION STATE ] Initialize all DPMS into INIT state (ready for setup)
// IMPORTANT: dpms[] is 1-based → valid range is [1..ROWS]
// =====================================================================
void initStatemachine()
{
  for (int id = 1; id <= ROWS; id++)
  {
    dpms[id].state = DPMState::Status::INIT; // ✅ updated
  }
  DBG_INFO("[CFG] ROWS=%d DPMS_SIZE=%d (dpms is 1-based)\n", ROWS, DPMS_SIZE);
}

// =====================================================================
// [SECTION STATE] Helper: debounce check
// Returns true if "condition" is continuously true for delayMs
// =====================================================================
bool debounceCheck(bool condition, unsigned long &timer, unsigned long delayMs);

// =====================================================================
// Safe write wrappers → enqueue Modbus commands via FreeRTOS queue
// Returns true if enqueue succeeded
// =====================================================================
bool safeWriteVoltage(int id, int value)
{
  return dpm_write_voltage(id, value) == 0;
}
bool safeWriteCurrent(int id, int value)
{
  return dpm_write_current(id, value) == 0;
}
bool safeWriteState(int id, bool on)
{
  return dpm_write_state(id, on) == 0;
}
// =====================================================================
// [SECTION STATE] Calculate the  Energy target value from Actual V/A
// =====================================================================

void dpm_calc_target_energy(int id)
{
  float volt = dpms[id].volt_set / 1000.0f;                // V
  float curr = dpms[id].cur_set / 1000.0f;                 // A
  dpms[id].energy_target = volt * curr * dpms[id].runtime; // Joules
  DBG_INFO("[CFG] DPM%d energy_target=%.2f J (%.2f V × %.2f A × %lus)\n",
           id, dpms[id].energy_target, volt, curr, dpms[id].runtime);
}
// =====================================================================
// [SECTION STATE] Ramp up Current or Pulse mode processing (PWM-like)
// =====================================================================

void dpm_update_curve(int id)
{
  DPMState &d = dpms[id];
  if (!d.curve.active)
    return;

  unsigned long now = millis();
  unsigned long elapsed = now - d.curve.start_ms;

  switch (d.curve.type)
  {
  case 1:
  { // --- Linear ramp ---
    if (elapsed >= d.curve.duration)
    {
      // ✅ Reached the end of ramp
      d.cur_set = d.curve.end_cur;
      safeWriteCurrent(id, d.cur_set); // ensure final target is sent once
      d.curve.active = false;          // mark curve complete
    }
    else
    {
      // intermediate ramp step
      float t = (float)elapsed / (float)d.curve.duration;
      d.cur_set = d.curve.start_cur + (int)((d.curve.end_cur - d.curve.start_cur) * t);
      safeWriteCurrent(id, d.cur_set); // send ramp update
    }
    break;
  }

  case 2:
  { // --- Pulse mode ---
    unsigned long phase = elapsed % d.curve.pulse_period;
    unsigned long half = d.curve.pulse_period / 2;
    int new_cur = (phase < half) ? d.curve.pulse_high : d.curve.pulse_low;
    if (new_cur != d.cur_set)
    {
      d.cur_set = new_cur;
      safeWriteCurrent(id, d.cur_set); // only send on change
    }
    break;
  }

  default:
    break;
  }
}

// =====================================================================
// [SECTION ENERGY] Calculate Energy
// =====================================================================
void dpm_update_energy(int id)
{
  unsigned long now = millis();
  if (dpms[id].last_energy_ms == 0)
  {
    dpms[id].last_energy_ms = now;
    return;
  }

  unsigned long dt = now - dpms[id].last_energy_ms;
  dpms[id].last_energy_ms = now;

  float volt = dpms[id].volt_act / 1000.0f;
  float curr = dpms[id].cur_act / 1000.0f;
  float power = volt * curr;

  // double joules = power * (dt / 1000.0);
  double joules = power * (dt / 1000.0);
  dpms[id].energy_temp += joules;
  dpms[id].energy_total += joules / 3600000.0;
  dpms[id].energy_anode += joules / 3600000.0;
  // Also keep convenient kWh counters

  // --- Service alert ---
  if (dpms[id].energy_anode > dpms[id].anode_threshold)
  {
    // char msgText[128];
    // snprintf(msgText, sizeof(msgText),"Service threshold reached (%.0f J total energy)", dpms[id].energy_anode);
    // mqtt_publish_event("service_due", 0, id, "Alert", msgText);
    dpms[id].anode_threshold += 100000.0; // avoid repeated spam (step up)
  }
}

// =====================================================================
// ------------------- State Handlers ----------------------------------
// [SECTION STATE]Each state has its own function → keeps logic modular & readable
// =====================================================================

// =====================================================================
// [SECTION STATE] Device is disabled,Invertet Relay Logig (relay On = DPM OFF)
// =====================================================================

void handleOff(int id)
{
  // Just idle. Skip Modbus reads/writes while OFF.
  dpms[id].remain_time = 0;// Set remain_time to 0
}
// =====================================================================
// [SECTION STATE] Overheat temperature is too high
// =====================================================================
void handleOverheat(int id)
{
  // dpms[id].remain_time = 0;
}

// ====================================================================
// [SECTION STATE] IDLE state: set safe V/I and wait for start
// =====================================================================
void handleIdle(int id)
{
  if (safeWriteVoltage(id, 300) && dpms[id].dpm_state == 1)
  {
    dpms[id].waitTimer = millis();
    dpms[id].state = DPMState::Status::WAIT_CURRENT; // ✅ updated
  }
}

// =====================================================================
// [SECTION STATE] Initial setup: write default V/I and turn ON
// =====================================================================
void handleInit(int id)
{
  if (safeWriteVoltage(id, 300) &&
      safeWriteCurrent(id, 300) &&
      safeWriteState(id, true))
  {
    dpms[id].state = DPMState::Status::WAIT_CURRENT; // ✅ updated
  }
  else
  {
    // dpms[id].state = DPMState::Status::ERROR_STATE; // ✅ updated (commented)
  }
}

// =====================================================================
// [SECTION STATE] Wait for current flow (meaning load is connected)
// =====================================================================
void handleWaitCurrent(int id)
{
  if (debounceCheck(dpms[id].dpm_state == 2, dpms[id].waitTimer, 2000))
  {
    if (safeWriteVoltage(id, dpms[id].volt_set) &&
        safeWriteCurrent(id, dpms[id].cur_set) &&
        safeWriteState(id, true))
    {
      // dpm_calc_target_energy(id);
      mqtt_publish_event("run_start", dpms[id].user, id, "INFO", "Process started");
      dpms[id].curve.active = true;              // enable curve processing
      dpms[id].curve.end_cur = dpms[id].cur_set; // ensure end_cur is set
      dpms[id].last_ms = millis();               // mark start of run
      dpms[id].waitTimer = millis();
      dpms[id].state = DPMState::Status::RUN; // ✅ updated
    }
    else
    {
      // dpms[id].state = DPMState::Status::ERROR_STATE; // ✅ updated (commented)
    }
  }
}

// =====================================================================
// [SECTION STATE] Wait Load Removed (no current flow)
// =====================================================================
void handleWaitRemove(int id)
{
  if (debounceCheck(dpms[id].dpm_state == 1, dpms[id].waitTimer, 2000))
  {
    dpms[id].state = DPMState::Status::WAIT_CURRENT; // ✅ updated
  }
}

// =====================================================================
// [SECTION STATE] Actively running: keep runtime countdown until done
// =====================================================================
void handleRun(int id)
{
  dpm_update_energy(id);
  dpm_update_curve(id);

  // If no curve active → hold last current
  if (!dpms[id].curve.active)
  {
    safeWriteCurrent(id, dpms[id].cur_set);
  }

  uint32_t now = millis();
  uint32_t elapsed = now - dpms[id].last_ms;
  uint32_t elapsed_s = elapsed / 1000;
  long remain = (long)dpms[id].runtime - (long)elapsed_s;
  if (remain < 0)
    remain = 0;
  dpms[id].remain_time = (uint32_t)remain;

  // --- ENERGY MODE ---
  if (dpms[id].mode == MODE_ENERGY)
  {

    // Normal energy reached before time
    if (dpms[id].energy_temp >= dpms[id].energy_target)
    {
      mqtt_publish_event("run_stop", dpms[id].user, id, "INFO", "Process stopped");
      safeWriteCurrent(id, dpms[id].idle_cur);
      dpms[id].state = DPMState::Status::WAIT_REMOVE; // ✅ updated
      dpms[id].remain_time = 0;
      // saveEnergyTotals();
      return;
    }

    // Time elapsed but energy not yet reached -> switch to CHECK_ENERGY
    if (elapsed >= dpms[id].runtime * 1000UL &&
        dpms[id].energy_temp < dpms[id].energy_target)
    {
      mqtt_publish_event("check_energy_phase", dpms[id].user, id, "INFO", "Energy phase check");
      dpms[id].state = DPMState::Status::CHECK_ENERGY; // ✅ updated
      DBG_INFO("[RUN] ✅ DPM %d entering CHECK_ENERGY (%.1f / %.1f J)\n",
               id, dpms[id].energy_temp, dpms[id].energy_target);
      return;
    }
  }

  // --- TIME MODE (unchanged) ---
  if (dpms[id].mode == MODE_TIME &&
      elapsed >= dpms[id].runtime * 1000UL)
  {
    mqtt_publish_event("run_stop", dpms[id].user, id, "INFO", "Process stopped");
    safeWriteCurrent(id, dpms[id].idle_cur);
    dpms[id].state = DPMState::Status::WAIT_REMOVE; // ✅ updated
    dpms[id].remain_time = 0;
    // saveEnergyTotals();
  }
}

// =====================================================================
// [SECTION STATE] Debounce implementation (to avoid spark)
// =====================================================================
bool debounceCheck(bool condition, unsigned long &timer, unsigned long delayMs)
{
  if (condition)
  {
    if (millis() - timer >= delayMs)
    {
      return true;
    }
  }
  else
  {
    timer = millis(); // reset whenever condition fails
  }
  return false;
}
// =====================================================================
// state Check Energy implementation
// =====================================================================
void handleCheckEnergy(int id)
{
  dpm_update_energy(id);

  // Check if energy target finally reached
  if (dpms[id].energy_temp >= dpms[id].energy_target)
  {
    DBG_INFO("[CHK] DPM %d energy reached after correction (%.1f J)\n",
             id, dpms[id].energy_temp);
    mqtt_publish_event("energy_reached_late", dpms[id].user, id, "INFO", "Energy target reached (late)");
    safeWriteCurrent(id, dpms[id].idle_cur);
    dpms[id].state = DPMState::Status::WAIT_REMOVE;
    dpms[id].remain_time = 0;
    // saveEnergyTotals();
    return;
  }

  // Timeout guard: if extra correction exceeds 10% of planned time
  unsigned long extra = millis() - (dpms[id].last_ms + dpms[id].runtime * 1000UL);
  if (extra > dpms[id].runtime * 100UL)
  { // 10% more time
    DBG_INFO("[CHK] DPM %d timeout (energy not reached, %.1f / %.1f J)\n",
             id, dpms[id].energy_temp, dpms[id].energy_target);
    mqtt_publish_event("energy_timeout", dpms[id].user, id, "", "Energy timeout reached");
    safeWriteCurrent(id, dpms[id].idle_cur);
    dpms[id].state = DPMState::Status::WAIT_REMOVE;
    // saveEnergyTotals();
  }
}

// =====================================================================
// [SECTION STATE] Main state machine dispatcher
// Loops through all devices [1..ROWS] and calls correct handler
// =====================================================================
void handle_StateMachine()
{
  for (int id = 1; id <= ROWS; id++)
  {
    switch (dpms[id].state)
    {
    case DPMState::Status::DPM_OFF:
      handleOff(id);
      break; // ✅ updated
    case DPMState::Status::IDLE:
      handleIdle(id);
      break; // ✅ updated
    case DPMState::Status::INIT:
      handleInit(id);
      break; // ✅ updated
    case DPMState::Status::WAIT_CURRENT:
      handleWaitCurrent(id);
      break; // ✅ updated
    case DPMState::Status::WAIT_REMOVE:
      handleWaitRemove(id);
      break; // ✅ updated
    case DPMState::Status::RUN:
      handleRun(id);
      break; // ✅ updated
    case DPMState::Status::CHECK_ENERGY:
      handleCheckEnergy(id);
      break; // ✅ updated
    case DPMState::Status::TEMP_HIGH:
      handleOverheat(id);
      break;                              // ✅ updated
    case DPMState::Status::CHECK_CONTACT: /* future */
      break;                              // ✅ updated
    case DPMState::Status::ERROR_STATE:   /* log error */
      break;                              // ✅ updated
    }
  }
}
