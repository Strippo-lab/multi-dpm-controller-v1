#include "config.h"
#include "mqtt_if.h"
#include "debug_log.h"
const int COLS = 15;
// ------------------------------------------------------------------
// Save dpms[1..ROWS] into Preferences (1-based keys)
// ------------------------------------------------------------------
void saveConfig() {
  Preferences prefs;
  prefs.begin("state", false);
  for (int id = 1; id <= ROWS; id++) {
    // ⚙️ state is now enum class → cast to int for storage
    prefs.putInt(("s_"   + String(id)).c_str(), static_cast<int>(dpms[id].state));
    prefs.putInt(("ds_"  + String(id)).c_str(), dpms[id].dpm_state);
    prefs.putInt(("va_"  + String(id)).c_str(), dpms[id].volt_act);
    prefs.putInt(("ca_"  + String(id)).c_str(), dpms[id].cur_act);
    prefs.putInt(("ta_"  + String(id)).c_str(), dpms[id].temp_act);
    prefs.putULong(("rt_" + String(id)).c_str(), dpms[id].remain_time);
    prefs.putInt(("vs_"  + String(id)).c_str(), dpms[id].volt_set);
    prefs.putInt(("cs_"  + String(id)).c_str(), dpms[id].cur_set);
    prefs.putInt(("ic_"  + String(id)).c_str(), dpms[id].idle_cur);
    prefs.putULong(("lm_" + String(id)).c_str(), dpms[id].last_ms);
    prefs.putULong(("ru_" + String(id)).c_str(), dpms[id].runtime);
    prefs.putULong(("wt_" + String(id)).c_str(), dpms[id].waitTimer);
    prefs.putInt(("pc_"  + String(id)).c_str(), dpms[id].percent);
    prefs.putInt(("r2_"  + String(id)).c_str(), dpms[id].reserved2);
    prefs.putInt(("r3_"  + String(id)).c_str(), dpms[id].reserved3);
    prefs.putDouble(("et_" + String(id)).c_str(), dpms[id].energy_total);
    prefs.putDouble(("ea_" + String(id)).c_str(), dpms[id].energy_anode);    
    prefs.putInt(("ct_"  + String(id)).c_str(), dpms[id].curve.type); // save curve type
    prefs.putInt(("usr_" + String(id)).c_str(), dpms[id].user);
    prefs.putInt(("lid_" + String(id)).c_str(), dpms[id].line_id); // Galvanic line ID   
  }  
  DBG_INFO("✅ Saved dpms[] to Preferences");
  prefs.end();
}

// ------------------------------------------------------------------
// Load dpms[1..ROWS] from Preferences (1-based keys)
// ------------------------------------------------------------------
void loadConfig() {
  Preferences prefs;
  prefs.begin("state", false);
  for (int id = 1; id <= ROWS; id++) {
    // ⚙️ state is now enum class → cast back from int to DPMState::Status
    dpms[id].state       = static_cast<DPMState::Status>(
                             prefs.getInt(("s_" + String(id)).c_str(), 0));
    dpms[id].dpm_state   = prefs.getInt(("ds_"  + String(id)).c_str(), 0);
    dpms[id].volt_act    = prefs.getInt(("va_"  + String(id)).c_str(), 0);
    dpms[id].cur_act     = prefs.getInt(("ca_"  + String(id)).c_str(), 0);
    dpms[id].temp_act    = prefs.getInt(("ta_"  + String(id)).c_str(), 0);
    dpms[id].remain_time = prefs.getULong(("rt_" + String(id)).c_str(), 0);
    dpms[id].volt_set    = prefs.getInt(("vs_"  + String(id)).c_str(), 0);
    dpms[id].cur_set     = prefs.getInt(("cs_"  + String(id)).c_str(), 0);
    dpms[id].idle_cur    = prefs.getInt(("ic_"  + String(id)).c_str(), 0);
    dpms[id].last_ms     = prefs.getULong(("lm_" + String(id)).c_str(), 0);
    dpms[id].runtime     = prefs.getULong(("ru_" + String(id)).c_str(), 0);
    dpms[id].waitTimer   = prefs.getULong(("wt_" + String(id)).c_str(), 0);
    dpms[id].percent     = prefs.getInt(("pc_"  + String(id)).c_str(), 0);
    dpms[id].reserved2   = prefs.getInt(("r2_"  + String(id)).c_str(), 0);
    dpms[id].reserved3   = prefs.getInt(("r3_"  + String(id)).c_str(), 0);
    dpms[id].energy_total = prefs.getDouble(("et_" + String(id)).c_str(), 0.0);
    dpms[id].energy_anode = prefs.getDouble(("ea_" + String(id)).c_str(), 0.0);
    dpms[id].curve.type  = prefs.getInt(("ct_" + String(id)).c_str(), 0); // load curve type
    dpms[id].user        = prefs.getInt(("usr_" + String(id)).c_str(), 0); //user ID 
    dpms[id].line_id     = prefs.getInt(("lid_" + String(id)).c_str(), 0); //line id          
  }
  DBG_INFO("📥 Loaded dpms[] from Preferences");
  prefs.end();  
}

// ------------------------------------------------------------------
// Debug print
// ------------------------------------------------------------------
void printConfig() {
  for (int id = 1; id <= ROWS; id++) {
    DBG_INFO(" ⚙️ DPM%d -> state:%d, dpm_state:%d, volt:%d, cur:%d, temp:%d, "
                  "volt_set:%d, cur_set:%d, idle_cur:%d, runtime:%lu, percent:%d\n",
                  id,
                  static_cast<int>(dpms[id].state), // ⚙️ cast enum class to int for print
                  dpms[id].dpm_state,
                  dpms[id].volt_act,
                  dpms[id].cur_act,
                  dpms[id].temp_act,
                  dpms[id].volt_set,
                  dpms[id].cur_set,
                  dpms[id].idle_cur,
                  dpms[id].runtime,
                  dpms[id].percent);
  }
}
