#pragma once
#include <stdint.h>

struct TH {
  float t;     // temperature (°C)
  float h;     // humidity (%)
  bool ok;     // valid reading 
};

void sensorInit(uint8_t dhtPin);
TH sensorRead();
