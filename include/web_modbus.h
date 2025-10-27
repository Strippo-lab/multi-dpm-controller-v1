#pragma once
#include "config.h"

// Start the tiny HTTP server (handlers in web_modbus.cpp)
void http_begin();
void http_poll();    // <-- add this
