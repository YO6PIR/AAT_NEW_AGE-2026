#include "PowerMeter.h"
#include "AppState.h"
#include "Storage.h"

namespace
{
  uint16_t powerFwdRaw10W;
  uint16_t powerFwdRaw100W;
  uint16_t powerDisplayW;
  unsigned long powerPeakTimestamp;
  bool powerDisplayValid;
  constexpr unsigned long RELATIVE_POWER_HOLD_MS = 800UL;
  constexpr int32_t SQRT_10_Q10 = 3238L;
  constexpr int32_t SQRT_100_Q10 = 10240L;
  constexpr int32_t SQRT_999_Q10 = 32366L;
  constexpr uint32_t Q10_SQUARED = 1048576UL;
}

bool initializeRelativePowerCalibration()
{
  const bool calibrationValid = loadPowerCalibration(powerFwdRaw10W, powerFwdRaw100W);
  if (!calibrationValid)
    loadFactoryPowerCalibration(powerFwdRaw10W, powerFwdRaw100W);
  return calibrationValid;
}

void setRelativePowerCalibration(uint16_t fwdRaw10W, uint16_t fwdRaw100W)
{
  if (isValidPowerCalibration(fwdRaw10W, fwdRaw100W))
  {
    powerFwdRaw10W = fwdRaw10W;
    powerFwdRaw100W = fwdRaw100W;
  }
  else
    loadFactoryPowerCalibration(powerFwdRaw10W, powerFwdRaw100W);
}

uint16_t relativePowerFromFwd(uint16_t fwdRaw)
{
  // A directional coupler reports RF amplitude.  Interpolate sqrt(power)
  // between the calibrated 10 W and 100 W amplitudes, then square in Q10.
  const int32_t rawOffset = (int32_t)fwdRaw - powerFwdRaw10W;
  const uint16_t rawSpan = powerFwdRaw100W - powerFwdRaw10W;
  int32_t scaledAmplitude = rawOffset * (SQRT_100_Q10 - SQRT_10_Q10);
  if (scaledAmplitude >= 0)
    scaledAmplitude += rawSpan / 2;
  else
    scaledAmplitude -= rawSpan / 2;

  int32_t amplitudeQ10 = SQRT_10_Q10 + scaledAmplitude / rawSpan;
  if (amplitudeQ10 <= 0)
    return 0;
  if (amplitudeQ10 > SQRT_999_Q10)
    amplitudeQ10 = SQRT_999_Q10;

  const uint32_t powerQ20 = (uint32_t)amplitudeQ10 * (uint32_t)amplitudeQ10;
  return (uint16_t)((powerQ20 + Q10_SQUARED / 2) / Q10_SQUARED);
}

void resetRelativePowerDisplay()
{
  powerDisplayW = 0;
  powerPeakTimestamp = 0;
  powerDisplayValid = false;
  RF_power = 0;
}

uint16_t updateRelativePowerDisplay(uint16_t fwdRaw)
{
  const uint16_t instantaneousPower = relativePowerFromFwd(fwdRaw);
  const unsigned long now = millis();

  if (!powerDisplayValid || instantaneousPower > powerDisplayW)
  {
    powerDisplayW = instantaneousPower;
    powerPeakTimestamp = now;
    powerDisplayValid = true;
  }
  else if ((unsigned long)(now - powerPeakTimestamp) >= RELATIVE_POWER_HOLD_MS)
    powerDisplayW = instantaneousPower;

  RF_power = powerDisplayW;
  return RF_power;
}
