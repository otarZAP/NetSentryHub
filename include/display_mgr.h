#pragma once
#include <Arduino.h>

void displayInit();
void displayUpdate(int nets, int probes, int alerts, int active_alerts,
                   int devices, uint8_t clients, bool ap_ok);
