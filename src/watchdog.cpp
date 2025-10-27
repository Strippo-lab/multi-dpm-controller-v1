#include "watchdog.h"
#ifdef WDT_ENABLE
  #include <esp_task_wdt.h>
#endif

static bool s_wdt_started = false;

void watchdog_start(uint32_t timeout_sec, bool panic) {
#ifdef WDT_ENABLE
  if (s_wdt_started) return;
  // Start Task WDT. 'panic=true' will print backtrace and reset on timeout.
  esp_task_wdt_init(timeout_sec, panic);
  s_wdt_started = true;
#endif
}

void watchdog_register_this_task() {
#ifdef WDT_ENABLE
  if (!s_wdt_started) return;
  // NULL means "current task"
  esp_task_wdt_add(NULL);
#endif
}

void watchdog_unregister_this_task() {
#ifdef WDT_ENABLE
  if (!s_wdt_started) return;
  esp_task_wdt_delete(NULL);
#endif
}

void watchdog_feed() {
#ifdef WDT_ENABLE
  if (!s_wdt_started) return;
  // Resets WDT *for the current task only*
  esp_task_wdt_reset();
#endif
}
