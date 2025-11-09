#pragma once

#include <stdint.h>

bool oledInit();

void oledShowMessage(const char* line1, const char* line2 = "");

void oledShowTempHum(float temp, float hum, bool ok);