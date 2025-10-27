#pragma once
#include <stdint.h>

// Start the non-blocking scan
void init_modbus_async_begin();

// Advance the scan by one step; returns true only when scan is finished
bool modbus_scan_step();

// True if scan is finished
bool modbus_scan_finished();

// Poll all found devices and update dpms[]
void readModbus();

// For UI / debug (used by web_modbus.cpp)
const char* modbus_cfg_name(int idx);
int         modbus_cfg_count();
