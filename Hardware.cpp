#include "Hardware.h"

#include <avr/io.h>

void hardwareInit()
{
  pinMode(PIN_C_UP, INPUT_PULLUP); pinMode(PIN_C_DOWN, INPUT_PULLUP);
  pinMode(PIN_TUNE_SWR, INPUT_PULLUP); pinMode(PIN_L_UP, INPUT_PULLUP);
  pinMode(PIN_L_DOWN, INPUT_PULLUP); pinMode(PIN_MODE, INPUT_PULLUP);
  pinMode(PIN_BAND_UP, INPUT_PULLUP); pinMode(PIN_BAND_DOWN, INPUT_PULLUP);
  pinMode(PIN_K1, OUTPUT); pinMode(PIN_L_CLOCK, OUTPUT);
  pinMode(PIN_SHARED_DATA, OUTPUT); pinMode(PIN_C_CLOCK, OUTPUT);
  digitalWrite(PIN_K1, LOW);
  analogReference(DEFAULT);
}

void k1Write(uint8_t value) { digitalWrite(PIN_K1, value ? HIGH : LOW); }
void lClockWrite(uint8_t value) { if (value) PORTC |= _BV(PC3); else PORTC &= (uint8_t)~_BV(PC3); }
void cClockWrite(uint8_t value) { if (value) PORTC |= _BV(PC5); else PORTC &= (uint8_t)~_BV(PC5); }
void sharedDataWrite(uint8_t value) { if (value) PORTC |= _BV(PC4); else PORTC &= (uint8_t)~_BV(PC4); }
void delay_ms_compat(uint16_t milliseconds) { delay(milliseconds); }
