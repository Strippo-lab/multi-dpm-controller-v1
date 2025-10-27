#pragma once
#include <Arduino.h>
#include <Client.h>
#include <IPAddress.h>

// Bring up Ethernet (DHCP). If DHCP fails and staticFallback==true,
// we’ll use the provided static params.
bool eth_setup(bool staticFallback = false,
               IPAddress ip   = IPAddress(0,0,0,0),
               IPAddress gw   = IPAddress(0,0,0,0),
               IPAddress mask = IPAddress(255,255,255,0),
               IPAddress dns  = IPAddress(0,0,0,0));

// Call this regularly from loop()
void eth_loop();

// Helpers
bool       eth_link_up();
IPAddress  eth_ip();

// Generic client you can pass to PubSubClient etc.
Client&    eth_client();

