#pragma once
#include <stdint.h>

void watchdog_start(uint32_t timeout_sec, bool panic = true);

// Call at the beginning of each FreeRTOS task you want watched
void watchdog_register_this_task();

// Optional: remove current task from WDT
void watchdog_unregister_this_task();

// Feed (reset) the WDT for *current* task
void watchdog_feed();