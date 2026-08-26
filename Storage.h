#pragma once

#include <Arduino.h>

enum EepromAddress : uint16_t {
  EE_ADDR_SWR_MIN = 0x0000, EE_ADDR_K1_MIN = 0x002C, EE_ADDR_CAP_MIN = 0x0042,
  EE_ADDR_IND_MIN = 0x0058, EE_ADDR_BAND_SELECTOR = 0x006E,
  EE_ADDR_DELAY_BEFORE_ADC = 0x0070, EE_ADDR_STRING10 = 0x0072,
  EE_ADDR_STRING9 = 0x0083, EE_ADDR_STRING8 = 0x008F, EE_ADDR_STRING7 = 0x00A0,
  EE_ADDR_STRING6 = 0x00AB, EE_ADDR_STRING5 = 0x00BC, EE_ADDR_STRING4 = 0x00CD,
  EE_ADDR_STRING3 = 0x00DA, EE_ADDR_BLANK = 0x00EB, EE_ADDR_STRING2 = 0x00FC,
  EE_ADDR_STRING1 = 0x010E, EE_ADDR_LEGACY_POWER_STEP = 0x0120,
  EE_ADDR_LEGACY_POWER_S10W = 0x0124, EE_ADDR_UREF = 0x0128,
  EE_ADDR_POWER_FWD_RAW_100W = 0x012A, EE_ADDR_POWER_FWD_RAW_10W = 0x012C,
  EE_LEGACY_LAYOUT_SIZE = 0x012E,
  EE_ADDR_COUPLER_SIGNATURE = 0x012E, EE_ADDR_COUPLER_VERSION = 0x0132,
  EE_ADDR_COUPLER_MARKER = 0x0133, EE_ADDR_COUPLER_FWD = 0x0134,
  EE_ADDR_COUPLER_REV = 0x0140, EE_ADDR_COUPLER_CHECKSUM = 0x014C,
  EE_ADDR_POWER_SIGNATURE = 0x014E, EE_ADDR_POWER_VERSION = 0x0152,
  EE_ADDR_POWER_MARKER = 0x0153, EE_ADDR_POWER_CHECKSUM = 0x0154,
  EE_LAYOUT_SIZE = 0x0156
};

constexpr uint8_t COUPLER_CALIBRATION_POINT_COUNT = 6;

struct CouplerCalibration
{
  uint16_t forward[COUPLER_CALIBRATION_POINT_COUNT];
  uint16_t reverse[COUPLER_CALIBRATION_POINT_COUNT];
};

uint16_t eeWord(uint16_t address); void eeWordSet(uint16_t address, uint16_t value);
uint32_t eeDword(uint16_t address); void eeDwordSet(uint16_t address, uint32_t value);
const char *eeText(uint16_t address);
uint16_t bandGet(); void bandSet(uint16_t value);
uint16_t indMinGet(uint8_t band); void indMinSet(uint8_t band, uint16_t value);
uint16_t capMinGet(uint8_t band); void capMinSet(uint8_t band, uint16_t value);
uint16_t k1MinGet(uint8_t band); void k1MinSet(uint8_t band, uint16_t value);
uint32_t swrMinGet(uint8_t band); void swrMinSet(uint8_t band, uint32_t value);
void loadFactoryCouplerCalibration(CouplerCalibration &calibration);
bool loadCouplerCalibration(CouplerCalibration &calibration);
bool saveCouplerCalibration(const CouplerCalibration &calibration);
bool isValidCouplerCalibration(const CouplerCalibration &calibration);
void loadFactoryPowerCalibration(uint16_t &fwdRaw10W, uint16_t &fwdRaw100W);
bool loadPowerCalibration(uint16_t &fwdRaw10W, uint16_t &fwdRaw100W);
bool savePowerCalibration(uint16_t fwdRaw10W, uint16_t fwdRaw100W);
bool isValidPowerCalibration(uint16_t fwdRaw10W, uint16_t fwdRaw100W);
