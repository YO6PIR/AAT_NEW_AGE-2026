#pragma once

#include <Arduino.h>
#include "Storage.h"

bool initializeCouplerCalibration();
void setActiveCouplerCalibration(const CouplerCalibration &calibration);
uint16_t getCouplerLeakage(uint16_t forwardRaw);
uint16_t getCorrectedRev(uint16_t forwardRaw, uint16_t reverseRaw);
uint16_t calculateSWRValue(uint16_t forwardRaw, uint16_t reverseForSWR);
void calculateSWR(uint32_t forwardRaw, uint32_t reverseRaw);
