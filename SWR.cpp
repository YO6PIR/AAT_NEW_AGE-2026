#include "SWR.h"
#include "AppState.h"
#include "Config.h"

namespace
{
CouplerCalibration activeCouplerCalibration;
}

bool initializeCouplerCalibration()
{
  const bool calibrationValid = loadCouplerCalibration(activeCouplerCalibration);
  if (!calibrationValid)
    loadFactoryCouplerCalibration(activeCouplerCalibration);
  return calibrationValid;
}

void setActiveCouplerCalibration(const CouplerCalibration &calibration)
{
  activeCouplerCalibration = calibration;
}

uint16_t getCouplerLeakage(uint16_t forwardRaw)
{
  const uint16_t firstForward = activeCouplerCalibration.forward[0];
  const uint16_t firstReverse = activeCouplerCalibration.reverse[0];
  if (forwardRaw < firstForward)
    return (uint16_t)(((uint32_t)forwardRaw * firstReverse) / firstForward);

  for (uint8_t i = 1; i < COUPLER_CALIBRATION_POINT_COUNT; ++i)
  {
    const uint16_t lowerForward = activeCouplerCalibration.forward[i - 1];
    const uint16_t lowerReverse = activeCouplerCalibration.reverse[i - 1];
    const uint16_t upperForward = activeCouplerCalibration.forward[i];
    const uint16_t upperReverse = activeCouplerCalibration.reverse[i];
    if (forwardRaw <= upperForward)
      return (uint16_t)(lowerReverse +
                        (((uint32_t)(forwardRaw - lowerForward) *
                          (upperReverse - lowerReverse)) /
                         (upperForward - lowerForward)));
  }

  return activeCouplerCalibration.reverse[COUPLER_CALIBRATION_POINT_COUNT - 1];
}

uint16_t getCorrectedRev(uint16_t forwardRaw, uint16_t reverseRaw)
{
  const uint16_t leakage = getCouplerLeakage(forwardRaw);
  return reverseRaw <= leakage ? 0 : reverseRaw - leakage;
}

uint16_t calculateSWRValue(uint16_t forwardRaw, uint16_t reverseForSWR)
{
  uint32_t tmp;
  if (forwardRaw == 0 || forwardRaw <= reverseForSWR)
    return 999;

  tmp = ((uint32_t)forwardRaw + reverseForSWR) * 100UL;
  tmp /= (forwardRaw - reverseForSWR);
  if (tmp > 999UL)
    tmp = 999UL;
  return (uint16_t)tmp;
}

void calculateSWR(uint32_t forwardRaw, uint32_t reverseRaw)
{
  const uint16_t fwdRaw = (uint16_t)forwardRaw;
  const uint16_t revRaw = (uint16_t)reverseRaw;

  // Keep the legacy RAW-derived SWR for all tuner decisions.
  swr = calculateSWRValue(fwdRaw, revRaw);

  // Compensation is a derived display value and never changes either RAW ADC value.
  const uint16_t compensatedReverse = getCorrectedRev(fwdRaw, revRaw);
  swrCompensated = calculateSWRValue(fwdRaw, compensatedReverse);
}
