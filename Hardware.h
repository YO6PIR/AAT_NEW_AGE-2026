#pragma once

#include <Arduino.h>

constexpr uint8_t PIN_C_UP = 20;
constexpr uint8_t PIN_C_DOWN = 9;
constexpr uint8_t PIN_TUNE_SWR = 13;
constexpr uint8_t PIN_L_UP = 8;
constexpr uint8_t PIN_L_DOWN = 21;
constexpr uint8_t PIN_MODE = 10;
constexpr uint8_t PIN_BAND_UP = 11;
constexpr uint8_t PIN_BAND_DOWN = 12;
constexpr uint8_t PIN_K1 = 3;
constexpr uint8_t PIN_L_CLOCK = A3;
constexpr uint8_t PIN_SHARED_DATA = A4;
constexpr uint8_t PIN_C_CLOCK = A5;

#define BUTTON_C_UP digitalRead(PIN_C_UP)
#define BUTTON_C_DOWN digitalRead(PIN_C_DOWN)
#define BUTTON_TUNE_SWR digitalRead(PIN_TUNE_SWR)
#define BUTTON_L_UP digitalRead(PIN_L_UP)
#define BUTTON_L_DOWN digitalRead(PIN_L_DOWN)
#define BUTTON_MODE digitalRead(PIN_MODE)
#define BUTTON_BAND_UP digitalRead(PIN_BAND_UP)
#define BUTTON_BAND_DOWN digitalRead(PIN_BAND_DOWN)

void hardwareInit();
void k1Write(uint8_t value);
void lClockWrite(uint8_t value);
void cClockWrite(uint8_t value);
void sharedDataWrite(uint8_t value);
void delay_ms_compat(uint16_t milliseconds);
