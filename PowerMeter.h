#pragma once

#include <Arduino.h>

bool initializeRelativePowerCalibration();
void setRelativePowerCalibration(uint16_t fwdRaw10W, uint16_t fwdRaw100W);
uint16_t relativePowerFromFwd(uint16_t fwdRaw);
uint16_t updateRelativePowerDisplay(uint16_t fwdRaw);
void resetRelativePowerDisplay();
