#pragma once
#include <Arduino.h>

// -----------------------------------------------------------
// Debug Level System
// -----------------------------------------------------------
// 0 = OFF
// 1 = ERRORS only
// 2 = WARNINGS + ERRORS
// 3 = INFO + WARNINGS + ERRORS (verbose)
// -----------------------------------------------------------

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 2   // default if not overridden
#endif

// -----------------------------------------------------------
// Base macro
// -----------------------------------------------------------
#define DBG_PRINT(level, label, fmt, ...) \
    do { \
        if (DEBUG_LEVEL >= level) { \
            Serial.printf("[%s] " fmt, label, ##__VA_ARGS__); \
        } \
    } while (0)

// -----------------------------------------------------------
// Level-specific macros
// -----------------------------------------------------------
#define DBG_ERROR(fmt, ...)  DBG_PRINT(1, "ERROR", fmt, ##__VA_ARGS__)
#define DBG_WARN(fmt, ...)   DBG_PRINT(2, "WARN ", fmt, ##__VA_ARGS__)
#define DBG_INFO(fmt, ...)   DBG_PRINT(3, "INFO ", fmt, ##__VA_ARGS__)
