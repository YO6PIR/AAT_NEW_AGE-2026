#pragma once

#include <Arduino.h>

//#if !defined(__AVR_ATmega328P__)
//#error "This firmware must be built for ATmega328P"
//#endif

//#if F_CPU != 8000000UL
//#error "This firmware must be built for an 8 MHz CPU clock"
//#endif

constexpr uint16_t MAX_SWR = 999;
constexpr uint8_t ON = 1;
constexpr uint8_t OFF = 0;
constexpr uint8_t SHIFT_REGISTER = 8;
constexpr uint16_t DELAY_MANUAL_TUNING = 200;
constexpr uint8_t DELAY_AFTER_RELAY = 13;
constexpr uint8_t DELAY_BEFORE_RELAY = 5;
constexpr uint8_t DELAY_SET_RELAY = 10;
constexpr uint8_t Ind = 0;
constexpr uint8_t Cap = 1;
constexpr uint8_t LC = 2;
constexpr uint8_t MAX_IND_CAP = 31;
constexpr uint8_t MAX_TUNE_PASSES = 4;
