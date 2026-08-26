#include "Measurements.h"
#include "AppState.h"
#include "Hardware.h"
#include "Storage.h"
#include "SWR.h"

void Samples()
{
  Vfwd = 0;
  Vrew = 0;
  delay_ms_compat(eeWord(EE_ADDR_DELAY_BEFORE_ADC));
  Vfwd = analogRead(A1);
  Vrew = analogRead(A2);
  calculateSWR(Vfwd, Vrew);
}
