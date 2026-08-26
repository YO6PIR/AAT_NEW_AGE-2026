#include "Storage.h"

#include <EEPROM.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stddef.h>

namespace
{
constexpr uint32_t COUPLER_SIGNATURE = 0x43504C52UL;
constexpr uint8_t COUPLER_VERSION = 1;
constexpr uint8_t COUPLER_VALID_MARKER = 0xA5;
constexpr uint32_t POWER_SIGNATURE = 0x50575243UL;
// Version 3 stores FWD RAW references; version 2 contained AD8307 ADC0 values.
constexpr uint8_t POWER_VERSION = 3;
constexpr uint8_t POWER_VALID_MARKER = 0x5A;
constexpr uint16_t FACTORY_POWER_FWD_RAW_10W = 300;
constexpr uint16_t FACTORY_POWER_FWD_RAW_100W = 867;

const uint16_t factoryCouplerForward[COUPLER_CALIBRATION_POINT_COUNT] PROGMEM = {136, 224, 297, 429, 657, 851};
const uint16_t factoryCouplerReverse[COUPLER_CALIBRATION_POINT_COUNT] PROGMEM = {3, 11, 19, 35, 66, 92};

uint16_t couplerChecksum(const CouplerCalibration &calibration)
{
  uint16_t checksum = 0x6D31;
  for (uint8_t i = 0; i < COUPLER_CALIBRATION_POINT_COUNT; ++i)
  {
    checksum = (uint16_t)((checksum << 5) | (checksum >> 11));
    checksum ^= calibration.forward[i];
    checksum = (uint16_t)((checksum << 5) | (checksum >> 11));
    checksum ^= calibration.reverse[i];
  }
  return checksum;
}

uint16_t powerChecksum(uint16_t fwdRaw10W, uint16_t fwdRaw100W)
{
  uint16_t checksum = 0x4B1D;
  checksum = (uint16_t)((checksum << 5) | (checksum >> 11));
  checksum ^= fwdRaw10W;
  checksum = (uint16_t)((checksum << 5) | (checksum >> 11));
  checksum ^= fwdRaw100W;
  return checksum;
}
}

struct __attribute__((packed)) FixedEepromImage
{
  uint32_t swrMin[11];
  uint16_t k1Min[11];
  uint16_t capMin[11];
  uint16_t indMin[11];
  uint16_t bandSelector;
  uint16_t delayBeforeAdc;
  char string10[17];
  char string9[12];
  char string8[17];
  char string7[11];
  char string6[17];
  char string5[17];
  char string4[13];
  char string3[17];
  char blank[17];
  char string2[18];
  char string1[18];
  float legacyPowerStep;
  float legacyPowerS10W;
  uint16_t uref;
  uint16_t powerFwdRaw100W;
  uint16_t powerFwdRaw10W;
  uint32_t couplerSignature;
  uint8_t couplerVersion;
  uint8_t couplerMarker;
  uint16_t couplerForward[COUPLER_CALIBRATION_POINT_COUNT];
  uint16_t couplerReverse[COUPLER_CALIBRATION_POINT_COUNT];
  uint16_t couplerChecksum;
  uint32_t powerSignature;
  uint8_t powerVersion;
  uint8_t powerMarker;
  uint16_t powerChecksum;
};

static_assert(offsetof(FixedEepromImage, swrMin) == EE_ADDR_SWR_MIN, "EEPROM swrMin address changed");
static_assert(offsetof(FixedEepromImage, k1Min) == EE_ADDR_K1_MIN, "EEPROM k1Min address changed");
static_assert(offsetof(FixedEepromImage, capMin) == EE_ADDR_CAP_MIN, "EEPROM capMin address changed");
static_assert(offsetof(FixedEepromImage, indMin) == EE_ADDR_IND_MIN, "EEPROM indMin address changed");
static_assert(offsetof(FixedEepromImage, bandSelector) == EE_ADDR_BAND_SELECTOR, "EEPROM band address changed");
static_assert(offsetof(FixedEepromImage, delayBeforeAdc) == EE_ADDR_DELAY_BEFORE_ADC, "EEPROM delay address changed");
static_assert(offsetof(FixedEepromImage, string10) == EE_ADDR_STRING10, "EEPROM string10 address changed");
static_assert(offsetof(FixedEepromImage, string9) == EE_ADDR_STRING9, "EEPROM string9 address changed");
static_assert(offsetof(FixedEepromImage, string8) == EE_ADDR_STRING8, "EEPROM string8 address changed");
static_assert(offsetof(FixedEepromImage, string7) == EE_ADDR_STRING7, "EEPROM string7 address changed");
static_assert(offsetof(FixedEepromImage, string6) == EE_ADDR_STRING6, "EEPROM string6 address changed");
static_assert(offsetof(FixedEepromImage, string5) == EE_ADDR_STRING5, "EEPROM string5 address changed");
static_assert(offsetof(FixedEepromImage, string4) == EE_ADDR_STRING4, "EEPROM string4 address changed");
static_assert(offsetof(FixedEepromImage, string3) == EE_ADDR_STRING3, "EEPROM string3 address changed");
static_assert(offsetof(FixedEepromImage, blank) == EE_ADDR_BLANK, "EEPROM blank address changed");
static_assert(offsetof(FixedEepromImage, string2) == EE_ADDR_STRING2, "EEPROM string2 address changed");
static_assert(offsetof(FixedEepromImage, string1) == EE_ADDR_STRING1, "EEPROM string1 address changed");
static_assert(offsetof(FixedEepromImage, legacyPowerStep) == EE_ADDR_LEGACY_POWER_STEP, "EEPROM legacy power step address changed");
static_assert(offsetof(FixedEepromImage, legacyPowerS10W) == EE_ADDR_LEGACY_POWER_S10W, "EEPROM legacy power S10W address changed");
static_assert(offsetof(FixedEepromImage, uref) == EE_ADDR_UREF, "EEPROM Uref address changed");
static_assert(offsetof(FixedEepromImage, powerFwdRaw100W) == EE_ADDR_POWER_FWD_RAW_100W, "EEPROM power FWD RAW 100W address changed");
static_assert(offsetof(FixedEepromImage, powerFwdRaw10W) == EE_ADDR_POWER_FWD_RAW_10W, "EEPROM power FWD RAW 10W address changed");
static_assert(offsetof(FixedEepromImage, couplerSignature) == EE_ADDR_COUPLER_SIGNATURE, "EEPROM coupler signature address changed");
static_assert(offsetof(FixedEepromImage, couplerVersion) == EE_ADDR_COUPLER_VERSION, "EEPROM coupler version address changed");
static_assert(offsetof(FixedEepromImage, couplerMarker) == EE_ADDR_COUPLER_MARKER, "EEPROM coupler marker address changed");
static_assert(offsetof(FixedEepromImage, couplerForward) == EE_ADDR_COUPLER_FWD, "EEPROM coupler FWD address changed");
static_assert(offsetof(FixedEepromImage, couplerReverse) == EE_ADDR_COUPLER_REV, "EEPROM coupler REV address changed");
static_assert(offsetof(FixedEepromImage, couplerChecksum) == EE_ADDR_COUPLER_CHECKSUM, "EEPROM coupler checksum address changed");
static_assert(offsetof(FixedEepromImage, powerSignature) == EE_ADDR_POWER_SIGNATURE, "EEPROM power signature address changed");
static_assert(offsetof(FixedEepromImage, powerVersion) == EE_ADDR_POWER_VERSION, "EEPROM power version address changed");
static_assert(offsetof(FixedEepromImage, powerMarker) == EE_ADDR_POWER_MARKER, "EEPROM power marker address changed");
static_assert(offsetof(FixedEepromImage, powerChecksum) == EE_ADDR_POWER_CHECKSUM, "EEPROM power checksum address changed");
static_assert(sizeof(FixedEepromImage) == EE_LAYOUT_SIZE, "EEPROM layout size changed");

FixedEepromImage fixedEepromImage EEMEM __attribute__((used)) = {
    {102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 0, 30, "   ...Saved!    ", "100W level=", "Press Button OK ", "10W level=", "OFF = fact.reset", "Release button! ", "Tuner OFF   ", "Delay Time Relay", "                ", " YO6PIR-2026(C) \n", "AAT-NEW-AGE 2026\n", 0.0f, 0.0f, 5000, FACTORY_POWER_FWD_RAW_100W, FACTORY_POWER_FWD_RAW_10W, 0, 0, 0, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0};

uint16_t eeWord(uint16_t a)
{
  uint16_t v;
  EEPROM.get(a, v);
  return v;
}
void eeWordSet(uint16_t a, uint16_t v) { EEPROM.put(a, v); }
uint32_t eeDword(uint16_t a)
{
  uint32_t v;
  EEPROM.get(a, v);
  return v;
}
void eeDwordSet(uint16_t a, uint32_t v) { EEPROM.put(a, v); }
const char *eeText(uint16_t a) { return (const char *)(uintptr_t)a; }
uint16_t bandGet() { return eeWord(EE_ADDR_BAND_SELECTOR); }
void bandSet(uint16_t v) { eeWordSet(EE_ADDR_BAND_SELECTOR, v); }
uint16_t indMinGet(uint8_t b) { return eeWord(EE_ADDR_IND_MIN + (uint16_t)b * sizeof(uint16_t)); }
void indMinSet(uint8_t b, uint16_t v) { eeWordSet(EE_ADDR_IND_MIN + (uint16_t)b * sizeof(uint16_t), v); }
uint16_t capMinGet(uint8_t b) { return eeWord(EE_ADDR_CAP_MIN + (uint16_t)b * sizeof(uint16_t)); }
void capMinSet(uint8_t b, uint16_t v) { eeWordSet(EE_ADDR_CAP_MIN + (uint16_t)b * sizeof(uint16_t), v); }
uint16_t k1MinGet(uint8_t b) { return eeWord(EE_ADDR_K1_MIN + (uint16_t)b * sizeof(uint16_t)); }
void k1MinSet(uint8_t b, uint16_t v) { eeWordSet(EE_ADDR_K1_MIN + (uint16_t)b * sizeof(uint16_t), v); }
uint32_t swrMinGet(uint8_t b) { return eeDword(EE_ADDR_SWR_MIN + (uint16_t)b * sizeof(uint32_t)); }
void swrMinSet(uint8_t b, uint32_t v) { eeDwordSet(EE_ADDR_SWR_MIN + (uint16_t)b * sizeof(uint32_t), v); }

void loadFactoryCouplerCalibration(CouplerCalibration &calibration)
{
  for (uint8_t i = 0; i < COUPLER_CALIBRATION_POINT_COUNT; ++i)
  {
    calibration.forward[i] = pgm_read_word(&factoryCouplerForward[i]);
    calibration.reverse[i] = pgm_read_word(&factoryCouplerReverse[i]);
  }
}

bool isValidCouplerCalibration(const CouplerCalibration &calibration)
{
  if (calibration.forward[0] == 0 || calibration.forward[0] > 1023 || calibration.reverse[0] > 1023)
    return false;
  for (uint8_t i = 1; i < COUPLER_CALIBRATION_POINT_COUNT; ++i)
  {
    if (calibration.forward[i] <= calibration.forward[i - 1] || calibration.forward[i] > 1023 || calibration.reverse[i] > 1023)
      return false;
  }
  return true;
}

bool loadCouplerCalibration(CouplerCalibration &calibration)
{
  uint32_t signature;
  uint8_t version;
  uint8_t marker;
  uint16_t storedChecksum;
  EEPROM.get(EE_ADDR_COUPLER_SIGNATURE, signature);
  EEPROM.get(EE_ADDR_COUPLER_VERSION, version);
  EEPROM.get(EE_ADDR_COUPLER_MARKER, marker);
  if (signature != COUPLER_SIGNATURE || version != COUPLER_VERSION || marker != COUPLER_VALID_MARKER)
    return false;
  EEPROM.get(EE_ADDR_COUPLER_FWD, calibration.forward);
  EEPROM.get(EE_ADDR_COUPLER_REV, calibration.reverse);
  EEPROM.get(EE_ADDR_COUPLER_CHECKSUM, storedChecksum);
  return isValidCouplerCalibration(calibration) && storedChecksum == couplerChecksum(calibration);
}

bool saveCouplerCalibration(const CouplerCalibration &calibration)
{
  if (!isValidCouplerCalibration(calibration))
    return false;
  EEPROM.update(EE_ADDR_COUPLER_MARKER, 0);
  EEPROM.put(EE_ADDR_COUPLER_SIGNATURE, COUPLER_SIGNATURE);
  EEPROM.put(EE_ADDR_COUPLER_VERSION, COUPLER_VERSION);
  EEPROM.put(EE_ADDR_COUPLER_FWD, calibration.forward);
  EEPROM.put(EE_ADDR_COUPLER_REV, calibration.reverse);
  const uint16_t checksum = couplerChecksum(calibration);
  EEPROM.put(EE_ADDR_COUPLER_CHECKSUM, checksum);
  EEPROM.update(EE_ADDR_COUPLER_MARKER, COUPLER_VALID_MARKER);
  return true;
}

void loadFactoryPowerCalibration(uint16_t &fwdRaw10W, uint16_t &fwdRaw100W)
{
  fwdRaw10W = FACTORY_POWER_FWD_RAW_10W;
  fwdRaw100W = FACTORY_POWER_FWD_RAW_100W;
}

bool isValidPowerCalibration(uint16_t fwdRaw10W, uint16_t fwdRaw100W)
{
  if (fwdRaw10W == 0 || fwdRaw10W > 1023 || fwdRaw100W > 1023 || fwdRaw100W <= fwdRaw10W)
    return false;
  // Reject the narrow legacy AD8307 span (factory values were 418/467).
  return fwdRaw100W - fwdRaw10W >= 64;
}

bool loadPowerCalibration(uint16_t &fwdRaw10W, uint16_t &fwdRaw100W)
{
  uint32_t signature;
  uint8_t version;
  uint8_t marker;
  uint16_t storedChecksum;
  EEPROM.get(EE_ADDR_POWER_SIGNATURE, signature);
  EEPROM.get(EE_ADDR_POWER_VERSION, version);
  EEPROM.get(EE_ADDR_POWER_MARKER, marker);
  if (signature != POWER_SIGNATURE || version != POWER_VERSION || marker != POWER_VALID_MARKER)
    return false;
  EEPROM.get(EE_ADDR_POWER_FWD_RAW_10W, fwdRaw10W);
  EEPROM.get(EE_ADDR_POWER_FWD_RAW_100W, fwdRaw100W);
  EEPROM.get(EE_ADDR_POWER_CHECKSUM, storedChecksum);
  return isValidPowerCalibration(fwdRaw10W, fwdRaw100W) && storedChecksum == powerChecksum(fwdRaw10W, fwdRaw100W);
}

bool savePowerCalibration(uint16_t fwdRaw10W, uint16_t fwdRaw100W)
{
  if (!isValidPowerCalibration(fwdRaw10W, fwdRaw100W))
    return false;
  EEPROM.update(EE_ADDR_POWER_MARKER, 0);
  EEPROM.put(EE_ADDR_POWER_FWD_RAW_10W, fwdRaw10W);
  EEPROM.put(EE_ADDR_POWER_FWD_RAW_100W, fwdRaw100W);
  EEPROM.put(EE_ADDR_POWER_SIGNATURE, POWER_SIGNATURE);
  EEPROM.put(EE_ADDR_POWER_VERSION, POWER_VERSION);
  EEPROM.put(EE_ADDR_POWER_CHECKSUM, powerChecksum(fwdRaw10W, fwdRaw100W));
  EEPROM.update(EE_ADDR_POWER_MARKER, POWER_VALID_MARKER);
  return true;
}
