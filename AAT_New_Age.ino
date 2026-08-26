/*
 * AAT New Age - Arduino functional port
 * Original: CodeVisionAVR / ATmega8A / 8 MHz internal RC oscillator
 * Target:   Arduino (MiniCore) / ATmega328P / 8 MHz internal RC oscillator
 *
 * This file intentionally preserves the original control flow, search
 * algorithm, tables, thresholds, arithmetic, EEPROM writes and delays.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <avr/pgmspace.h>
#include <util/delay.h> // Relay clock pulses need deterministic 10 us timing.
#include "Storage.h"
#include "Hardware.h"
#include "Config.h"
#include "AppState.h"
#include "Measurements.h"
#include "PowerMeter.h"
#include "SWR.h"

static LiquidCrystal lcd(0, 1, 2, 4, 5, 6, 7);
static void lcd_init(uint8_t columns) { lcd.begin(columns, 2); }
static void lcd_clear(void) { lcd.clear(); }
static void lcd_gotoxy(uint8_t x, uint8_t y) { lcd.setCursor(x, y); }
static void lcd_putchar(char value)
{
  if (value == '\n')
    lcd_gotoxy(0, 1);
  else
    lcd.write((uint8_t)value);
}
static void lcd_puts_P(const char *text)
{
  char value;
  while ((value = (char)pgm_read_byte(text++)) != 0)
    lcd_putchar(value);
}

#define LCD_PUTS(text) lcd_puts_P(PSTR(text))

static void lcd_putse(const char *address)
{
  char value;
  while ((value = (char)EEPROM.read((int)(uintptr_t)address++)) != 0)
    lcd_putchar(value);
}

// ---------------- Original tables and state ----------------
const uint8_t L_table_coarsie[32] PROGMEM = {0, 1, 2, 4, 5, 7, 11, 18, 14, 23, 28, 33, 40, 64, 65, 67, 128, 136, 144, 96, 100, 160, 164, 176, 208, 216, 224, 226, 232, 242, 247, 255};
const uint8_t C_table_coarsie[32] PROGMEM = {0, 1, 2, 4, 5, 8, 11, 14, 18, 23, 28, 33, 39, 46, 53, 60, 68, 77, 86, 95, 105, 116, 127, 138, 150, 163, 176, 189, 203, 218, 233, 248};
const uint8_t L_table_real[32] PROGMEM = {0, 1, 2, 3, 4, 5, 7, 8, 9, 12, 13, 15, 17, 19, 20, 22, 24, 28, 30, 32, 35, 37, 40, 43, 49, 53, 56, 58, 60, 64, 68, 72};
const uint16_t C_table_real[32] PROGMEM = {0, 5, 10, 22, 27, 31, 47, 64, 74, 102, 119, 129, 161, 188, 216, 243, 274, 311, 348, 386, 413, 463, 510, 537, 592, 634, 684, 744, 795, 854, 909, 969};

static uint8_t lCode(uint8_t i) { return pgm_read_byte(&L_table_coarsie[i]); }
static uint8_t cCode(uint8_t i) { return pgm_read_byte(&C_table_coarsie[i]); }
static uint8_t lReal(uint8_t i) { return pgm_read_byte(&L_table_real[i]); }
static uint16_t cReal(uint8_t i) { return pgm_read_word(&C_table_real[i]); }

const uint8_t char0[8] PROGMEM = {0b10011100, 0b10011000, 0b10010000, 0b10000000, 0b10011000, 0b10010100, 0b10011000, 0b10010100};
const uint8_t char1[8] PROGMEM = {0b10000000, 0b10000000, 0b10010101, 0b10000000, 0b10000000, 0b10000000, 0b10010101, 0b10000000};
const uint8_t char2[8] PROGMEM = {0b10000000, 0b10010101, 0b10010101, 0b10010101, 0b10000000, 0b10000000, 0b10010101, 0b10000000};
const uint8_t char3[8] PROGMEM = {0b10000000, 0b10000000, 0b10010101, 0b10000000, 0b10000000, 0b10010101, 0b10010101, 0b10010101};
const uint8_t char4[8] PROGMEM = {0b10000000, 0b10010101, 0b10010101, 0b10010101, 0b10000000, 0b10010101, 0b10010101, 0b10010101};

static void define_char(const uint8_t *pc, uint8_t char_code)
{
  uint8_t bitmap[8];
  for (uint8_t i = 0; i < 8; ++i)
    bitmap[i] = pgm_read_byte(pc++);
  lcd.createChar(char_code, bitmap);
}

static void loadNormalBarGlyphs(void)
{
  define_char(char0, 0);
  define_char(char1, 1);
  define_char(char2, 2);
  define_char(char3, 3);
  define_char(char4, 4);
}

static void Send_LC(uint8_t tmp1, uint8_t tmp)
{
  // ORIGINAL TIMING - candidate for future optimization.
  delay_ms_compat(DELAY_BEFORE_RELAY);
  // Keep the short, timing-critical shift transaction free from Timer0 ISR jitter.
  noInterrupts();
  if (send == Ind || send == LC)
  {
    for (uint8_t i = 0; i < SHIFT_REGISTER; ++i)
    {
      sharedDataWrite((tmp1 & 0x80) ? ON : OFF);
      tmp1 <<= 1;
      lClockWrite(OFF);
      _delay_us(DELAY_SET_RELAY);
      lClockWrite(ON);
      _delay_us(DELAY_SET_RELAY);
    }
  }
  if (send == Cap || send == LC)
  {
    for (uint8_t i = 0; i < SHIFT_REGISTER; ++i)
    {
      sharedDataWrite((tmp & 0x80) ? ON : OFF);
      tmp <<= 1;
      cClockWrite(OFF);
      _delay_us(DELAY_SET_RELAY);
      cClockWrite(ON);
      _delay_us(DELAY_SET_RELAY);
    }
  }
  interrupts();
  delay_ms_compat(DELAY_AFTER_RELAY);
}

static void swr_out(void)
{
  if (swrCompensated > 900 || swrCompensated < 1)
  {
    LCD_PUTS(">>>>");
    swrCompensated = 999;
  }
  else
  {
    lcd_putchar((swrCompensated / 100) + '0');
    LCD_PUTS(",");
    lcd_putchar(((swrCompensated / 10) % 10) + '0');
    lcd_putchar((swrCompensated % 10) + '0');
  }
}

static void Formateaza(int16_t tmp)
{
  if (!sar)
  {
    if (tmp / 1000)
      lcd_putchar(tmp / 1000 + '0');
    else
      LCD_PUTS(" ");
  }
  if (tmp / 100)
    lcd_putchar((tmp / 100) % 10 + '0');
  else
    LCD_PUTS(" ");
  if (tmp / 10)
    lcd_putchar((tmp / 10) % 10 + '0');
  else
    LCD_PUTS(" ");
  lcd_putchar(tmp % 10 + '0');
  sar = false;
}

static void l_out(int16_t inductor)
{
  if (sweep_relay)
    lcd_gotoxy(6, 0);
  else
    lcd_gotoxy(0, 0);
  if (inductor < 10)
  {
    LCD_PUTS("0,");
    lcd_putchar(inductor + '0');
  }
  else
  {
    lcd_putchar(inductor / 10 + '0');
    LCD_PUTS(",");
    lcd_putchar(inductor % 10 + '0');
  }
  LCD_PUTS("uH");
}

static void c_out(int16_t capacity)
{
  if (sweep_relay)
    lcd_gotoxy(0, 0);
  else
    lcd_gotoxy(6, 0);
  if (capacity / 100)
    lcd_putchar((capacity / 100) % 10 + '0');
  else
    LCD_PUTS(" ");
  if (capacity / 10)
    lcd_putchar((capacity / 10) % 10 + '0');
  else
    LCD_PUTS(" ");
  lcd_putchar(capacity % 10 + '0');
  LCD_PUTS("pF");
}

static bool loadTuneMemory(uint8_t band, uint8_t &rememberedL, uint8_t &rememberedC,
                           uint8_t &rememberedK1, uint16_t &rememberedSWR)
{
  const uint16_t storedL = indMinGet(band);
  const uint16_t storedC = capMinGet(band);
  const uint16_t storedK1 = k1MinGet(band);
  const uint32_t storedSWR = swrMinGet(band);
  if (storedL > MAX_IND_CAP || storedC > MAX_IND_CAP || storedK1 > 1 ||
      storedSWR < 100 || storedSWR >= MAX_SWR)
    return false;

  rememberedL = (uint8_t)storedL;
  rememberedC = (uint8_t)storedC;
  rememberedK1 = (uint8_t)storedK1;
  rememberedSWR = (uint16_t)storedSWR;
  return true;
}

static void tunner(void)
{
  const uint16_t storedBand = bandGet();
  const uint8_t band = storedBand <= 10 ? (uint8_t)storedBand : 10;
  uint8_t rememberedL = 0;
  uint8_t rememberedC = 0;
  uint8_t rememberedK1 = 0;
  uint16_t rememberedSWR = MAX_SWR;
  if (loadTuneMemory(band, rememberedL, rememberedC, rememberedK1, rememberedSWR))
  {
    L = rememberedL;
    C = rememberedC;
    sweep_relay = rememberedK1;
    swr = rememberedSWR;
  }
  else
  {
    L = 0;
    C = 0;
    sweep_relay = 0;
    swr = MAX_SWR;
  }
  k1Write(sweep_relay);
  send = LC;
  Send_LC(lCode(L), cCode(C));
  c_out(cReal(C));
  l_out(lReal(L));
  lcd_gotoxy(5, 0);
  LCD_PUTS("-");
}

static void drawBandStatus(void)
{
  uint16_t band = bandGet();
  lcd_gotoxy(11, 0);
  switch (band)
  {
  case 0:
    LCD_PUTS("  80m");
    break;
  case 1:
    LCD_PUTS("  60m");
    break;
  case 2:
    LCD_PUTS("  40m");
    break;
  case 3:
    LCD_PUTS("  30m");
    break;
  case 4:
    LCD_PUTS("  20m");
    break;
  case 5:
    LCD_PUTS("  17m");
    break;
  case 6:
    LCD_PUTS("  15m");
    break;
  case 7:
    LCD_PUTS("  12m");
    break;
  case 8:
    LCD_PUTS("  11m");
    break;
  case 9:
    LCD_PUTS("  10m");
    break;
  case 10:
    LCD_PUTS("  AUX");
    break;
  default:
    bandSet(10);
    LCD_PUTS("  AUX");
    break;
  }
}

static void band_out(void)
{
  drawBandStatus();
  tunner();
}

static void bargraf(uint8_t Pos, uint8_t Row, uint8_t Maxbar, uint16_t Adc1, uint16_t Adc2,
                    bool showFr = true)
{
  uint16_t Scala = 1023 / Maxbar;
  uint8_t Aplin = Adc1 / Scala;
  uint8_t Bplin = Adc2 / Scala;
  uint8_t Agol = Maxbar - Aplin;
  uint8_t Bgol = Maxbar - Bplin;
  uint8_t Aplin2 = Aplin + 1;
  uint8_t Bplin2 = Bplin + 1;
  const uint8_t barPos = showFr ? Pos + 1 : Pos;
  if (showFr)
  {
    lcd_gotoxy(Pos, Row);
    lcd_putchar(0);
  }
  lcd_gotoxy(barPos, Row);
  if (Aplin || Bplin)
  {
    if (Aplin > Bplin)
    {
      for (uint8_t X = 1; X <= Bplin; ++X)
        lcd_putchar(4);
      for (uint8_t X = Bplin2; X <= Aplin; ++X)
        lcd_putchar(2);
      for (uint8_t Y = 1; Y <= Agol; ++Y)
        lcd_putchar(1);
    }
    if (Bplin > Aplin)
    {
      for (uint8_t X = 1; X <= Aplin; ++X)
        lcd_putchar(4);
      for (uint8_t X = Aplin2; X <= Bplin; ++X)
        lcd_putchar(3);
      for (uint8_t Y = 1; Y <= Bgol; ++Y)
        lcd_putchar(1);
    }
    if (Aplin == Bplin)
    {
      for (uint8_t X = 1; X <= Aplin; ++X)
        lcd_putchar(4);
      for (uint8_t Y = 1; Y <= Agol; ++Y)
        lcd_putchar(1);
    }
  }
  else
    for (uint8_t X = 1; X <= Maxbar; ++X)
      lcd_putchar(1);
}

// SWR/PWR uses the original dual renderer across all 16 LCD positions.
// Its fine scale is three visual bars per segment, with no explicit hold.
constexpr uint16_t FWD_BAR_DECAY_MS = 60;
constexpr uint8_t FWD_BAR_SEGMENTS = 16;
constexpr uint8_t FWD_BAR_FINE_UNITS = FWD_BAR_SEGMENTS * 3;
constexpr uint16_t FWD_BAR_DECAY_STEP_RAW =
    3 * ((1023UL + FWD_BAR_FINE_UNITS - 1) / FWD_BAR_FINE_UNITS);
static uint16_t displayedFwd;
static unsigned long fwdBarDecayTimestamp;

static void resetFwdBarEnvelope(void)
{
  displayedFwd = 0;
  fwdBarDecayTimestamp = millis();
}

static uint16_t updateFwdBarEnvelope(uint16_t fwdRaw)
{
  const unsigned long now = millis();

  // Attack immediately on every new FWD peak.
  if (fwdRaw >= displayedFwd)
  {
    displayedFwd = fwdRaw;
    fwdBarDecayTimestamp = now;
  }
  else if ((unsigned long)(now - fwdBarDecayTimestamp) >= FWD_BAR_DECAY_MS)
  {
    // Never decay below the current raw FWD input; this keeps a CW carrier
    // stable while RF-off follows the same non-blocking slope down to zero.
    const uint16_t gap = displayedFwd - fwdRaw;
    displayedFwd -= gap <= FWD_BAR_DECAY_STEP_RAW ? gap : FWD_BAR_DECAY_STEP_RAW;
    fwdBarDecayTimestamp = now;
  }

  return displayedFwd;
}

static void prepareSWRDisplay(void)
{
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("SWR:");
  lcd_gotoxy(8, 0);
  LCD_PUTS("  P=");
}

enum CouplerMenuOption : uint8_t
{
  COUPLER_NEW_CAL,
  COUPLER_DEFAULTS,
  COUPLER_CANCEL
};

static uint8_t couplerSelectOption(void)
{
  uint8_t option = COUPLER_NEW_CAL;
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("COUPLER CAL");

  for (;;)
  {
    lcd_gotoxy(0, 1);
    if (option == COUPLER_NEW_CAL)
      LCD_PUTS("NEW CAL         ");
    else if (option == COUPLER_DEFAULTS)
      LCD_PUTS("DEFAULT         ");
    else
      LCD_PUTS("CANCEL          ");

    if (!BUTTON_BAND_UP && option < COUPLER_CANCEL)
    {
      ++option;
      delay_ms_compat(150);
    }
    if (!BUTTON_BAND_DOWN && option > COUPLER_NEW_CAL)
    {
      --option;
      delay_ms_compat(150);
    }
    if (!BUTTON_TUNE_SWR)
    {
      while (!BUTTON_TUNE_SWR)
      {
      }
      delay_ms_compat(200);
      return option;
    }
  }
}

static bool couplerSaveConfirm(void)
{
  bool yes = false;
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("SAVE COUPLER?");

  for (;;)
  {
    lcd_gotoxy(0, 1);
    if (yes)
      LCD_PUTS("YES             ");
    else
      LCD_PUTS("NO              ");
    if (!BUTTON_BAND_UP)
    {
      yes = true;
      delay_ms_compat(150);
    }
    if (!BUTTON_BAND_DOWN)
    {
      yes = false;
      delay_ms_compat(150);
    }
    if (!BUTTON_TUNE_SWR)
    {
      while (!BUTTON_TUNE_SWR)
      {
      }
      delay_ms_compat(200);
      return yes;
    }
  }
}

static void couplerCalibration(void)
{
  const uint8_t option = couplerSelectOption();
  if (option == COUPLER_CANCEL)
    return;

  if (option == COUPLER_DEFAULTS)
  {
    CouplerCalibration factoryCalibration;
    loadFactoryCouplerCalibration(factoryCalibration);
    if (saveCouplerCalibration(factoryCalibration))
    {
      setActiveCouplerCalibration(factoryCalibration);
      lcd_clear();
      LCD_PUTS("DEFAULTS SAVED");
    }
    else
    {
      lcd_clear();
      LCD_PUTS("DEFAULT ERROR");
    }
    delay_ms_compat(1200);
    return;
  }

  CouplerCalibration pendingCalibration;
  for (uint8_t point = 0; point < COUPLER_CALIBRATION_POINT_COUNT; ++point)
  {
    for (;;)
    {
      Samples();
      lcd_gotoxy(0, 0);
      LCD_PUTS("CAL POINT ");
      lcd_putchar('1' + point);
      LCD_PUTS("/6   ");
      lcd_gotoxy(0, 1);
      LCD_PUTS("F");
      sar = false;
      Formateaza((int16_t)Vfwd);
      LCD_PUTS(" R");
      sar = false;
      Formateaza((int16_t)Vrew);
      LCD_PUTS("     ");

      if (!BUTTON_TUNE_SWR)
      {
        const uint16_t capturedForward = (uint16_t)Vfwd;
        const uint16_t capturedReverse = (uint16_t)Vrew;
        while (!BUTTON_TUNE_SWR)
        {
        }
        if (capturedForward == 0 || (point > 0 && capturedForward <= pendingCalibration.forward[point - 1]))
        {
          lcd_clear();
          lcd_gotoxy(0, 0);
          LCD_PUTS("FWD NOT HIGHER");
          lcd_gotoxy(0, 1);
          LCD_PUTS("ADJUST + ENTER");
          delay_ms_compat(1200);
          continue;
        }
        pendingCalibration.forward[point] = capturedForward;
        pendingCalibration.reverse[point] = capturedReverse;
        delay_ms_compat(200);
        break;
      }
    }
  }

  if (!couplerSaveConfirm())
  {
    lcd_clear();
    LCD_PUTS("CAL DISCARDED");
    delay_ms_compat(1200);
    return;
  }

  if (saveCouplerCalibration(pendingCalibration))
  {
    setActiveCouplerCalibration(pendingCalibration);
    lcd_clear();
    LCD_PUTS("COUPLER SAVED");
  }
  else
  {
    lcd_clear();
    LCD_PUTS("CAL SAVE ERROR");
  }
  delay_ms_compat(1200);
}

enum PowerMenuOption : uint8_t
{
  POWER_NEW_CAL,
  POWER_DEFAULTS,
  POWER_CANCEL
};

static uint8_t powerSelectOption(void)
{
  uint8_t option = POWER_NEW_CAL;
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("POWER CAL");
  for (;;)
  {
    lcd_gotoxy(0, 1);
    if (option == POWER_NEW_CAL)
      LCD_PUTS("NEW CAL         ");
    else if (option == POWER_DEFAULTS)
      LCD_PUTS("DEFAULT         ");
    else
      LCD_PUTS("CANCEL          ");
    if (!BUTTON_BAND_UP && option < POWER_CANCEL)
    {
      ++option;
      delay_ms_compat(150);
    }
    if (!BUTTON_BAND_DOWN && option > POWER_NEW_CAL)
    {
      --option;
      delay_ms_compat(150);
    }
    if (!BUTTON_TUNE_SWR)
    {
      while (!BUTTON_TUNE_SWR)
      {
      }
      delay_ms_compat(200);
      return option;
    }
  }
}

static bool powerSaveConfirm(void)
{
  bool yes = false;
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("SAVE POWER CAL?");
  for (;;)
  {
    lcd_gotoxy(0, 1);
    if (yes)
      LCD_PUTS("YES             ");
    else
      LCD_PUTS("NO              ");
    if (!BUTTON_BAND_UP)
    {
      yes = true;
      delay_ms_compat(150);
    }
    if (!BUTTON_BAND_DOWN)
    {
      yes = false;
      delay_ms_compat(150);
    }
    if (!BUTTON_TUNE_SWR)
    {
      while (!BUTTON_TUNE_SWR)
      {
      }
      delay_ms_compat(200);
      return yes;
    }
  }
}

static uint16_t capturePowerRaw(bool hundredWatts)
{
  while (!BUTTON_TUNE_SWR)
  {
  }
  for (;;)
  {
    Samples();
    lcd_gotoxy(0, 0);
    if (hundredWatts)
      LCD_PUTS("SET 100W        ");
    else
      LCD_PUTS("SET 10W         ");
    lcd_gotoxy(0, 1);
    LCD_PUTS("RAW ");
    sar = false;
    Formateaza((int16_t)Vfwd);
    LCD_PUTS("       ");
    if (!BUTTON_TUNE_SWR)
    {
      const uint16_t captured = (uint16_t)Vfwd;
      while (!BUTTON_TUNE_SWR)
      {
      }
      delay_ms_compat(200);
      return captured;
    }
  }
}

static void powerCalibration(void)
{
  const uint8_t option = powerSelectOption();
  if (option == POWER_CANCEL)
    return;
  if (option == POWER_DEFAULTS)
  {
    uint16_t fwdRaw10W;
    uint16_t fwdRaw100W;
    loadFactoryPowerCalibration(fwdRaw10W, fwdRaw100W);
    if (savePowerCalibration(fwdRaw10W, fwdRaw100W))
    {
      setRelativePowerCalibration(fwdRaw10W, fwdRaw100W);
      lcd_clear();
      LCD_PUTS("POWER DEFAULT");
    }
    else
    {
      lcd_clear();
      LCD_PUTS("POWER DEF ERROR");
    }
    delay_ms_compat(1200);
    return;
  }

  const uint16_t fwdRaw10W = capturePowerRaw(false);
  const uint16_t fwdRaw100W = capturePowerRaw(true);
  if (!isValidPowerCalibration(fwdRaw10W, fwdRaw100W))
  {
    lcd_clear();
    LCD_PUTS("POWER CAL ERROR");
    delay_ms_compat(1200);
    return;
  }
  if (!powerSaveConfirm())
  {
    lcd_clear();
    LCD_PUTS("CAL DISCARDED");
    delay_ms_compat(1200);
    return;
  }
  if (savePowerCalibration(fwdRaw10W, fwdRaw100W))
  {
    setRelativePowerCalibration(fwdRaw10W, fwdRaw100W);
    lcd_clear();
    LCD_PUTS("POWER SAVED");
  }
  else
  {
    lcd_clear();
    LCD_PUTS("POWER SAVE ERROR");
  }
  delay_ms_compat(1200);
}

constexpr uint16_t VREF_FACTORY_MV = 5000;
constexpr uint16_t VREF_MIN_MV = 4500;
constexpr uint16_t VREF_MAX_MV = 5500;
static uint16_t activeVrefMv = VREF_FACTORY_MV;

static bool isValidVref(uint16_t vref)
{
  return vref >= VREF_MIN_MV && vref <= VREF_MAX_MV;
}

static bool isValidStoredTunerState(void)
{
  const uint16_t band = bandGet();
  return band <= 10 && capMinGet((uint8_t)band) <= MAX_IND_CAP &&
         indMinGet((uint8_t)band) <= MAX_IND_CAP && k1MinGet((uint8_t)band) <= 1;
}

static void drawVrefValue(uint16_t vref)
{
  lcd_gotoxy(8, 0);
  sar = false;
  Formateaza(vref);
}

static void setari(void)
{
  lcd_clear();
  lcd_gotoxy(0, 0);
  lcd_putse(eeText(EE_ADDR_STRING5));
  while (!BUTTON_TUNE_SWR)
  {
  }

  const uint16_t savedVref = eeWord(EE_ADDR_UREF);
  activeVrefMv = isValidVref(savedVref) ? savedVref : VREF_FACTORY_MV;
  uint16_t uref = activeVrefMv;
  lcd_clear();
  // Static labels are drawn once; only the editable value is redrawn.
  lcd_gotoxy(0, 0);
  LCD_PUTS(" Uref = ");
  lcd_gotoxy(12, 0);
  LCD_PUTS("mV  ");
  lcd_gotoxy(0, 1);
  lcd_putse(eeText(EE_ADDR_STRING6));
  drawVrefValue(uref);

  while (BUTTON_TUNE_SWR)
  {
    const uint16_t previousVref = uref;
    if (!BUTTON_BAND_UP && uref <= VREF_MAX_MV - 10)
      uref += 10;
    if (!BUTTON_BAND_DOWN && uref >= VREF_MIN_MV + 10)
      uref -= 10;

    if (uref != previousVref)
      drawVrefValue(uref);

    delay_ms_compat(200);
  }

  eeWordSet(EE_ADDR_UREF, uref);
  activeVrefMv = uref;
  powerCalibration();
  couplerCalibration();
  lcd_clear();
}

struct TuneBest
{
  uint16_t swr;
  uint8_t l;
  uint8_t c;
  uint8_t k1;
};

enum TuneSearchResult : uint8_t
{
  SEARCH_CONTINUE,
  SEARCH_GOOD_MATCH,
  SEARCH_ABORTED,
  SEARCH_HIGH_POWER,
  SEARCH_RF_LOST
};

constexpr uint16_t AUTO_TUNE_MAX_POWER_W = 15;
constexpr uint16_t AUTO_TUNE_HIGH_POWER_WARNING_MS = 600;
constexpr uint16_t ALREADY_MATCHED_SWR = 130;
constexpr uint16_t AUTO_TUNE_MATCH_MESSAGE_MS = 900;
constexpr unsigned long AUTO_TUNE_REQUEST_HOLD_MS = 350UL;
constexpr unsigned long RF_TUNE_ARM_HOLD_MS = 3000UL;
constexpr uint8_t RF_TUNE_VALID_SAMPLES = 3;

enum AutoTuneState : uint8_t
{
  AUTO_TUNE_IDLE,
  AUTO_TUNE_ARMING,
  AUTO_TUNE_REQUESTED,
  AUTO_TUNE_RF_ARM_PRESS,
  AUTO_TUNE_ACTIVE
};

static AutoTuneState autoTuneState = AUTO_TUNE_IDLE;
static unsigned long autoTuneButtonPressStart = 0;
static bool rfTuneArmed = false;
static uint8_t rfTuneValidSamples = 0;

static bool isAutoTunePowerTooHigh(void)
{
  return relativePowerFromFwd((uint16_t)Vfwd) > AUTO_TUNE_MAX_POWER_W;
}

static bool isAutoTuneRfPresent(void)
{
  return relativePowerFromFwd((uint16_t)Vfwd) > 0;
}

static void showAutoTuneHighPowerWarning(void)
{
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("  HIGH POWER !  ");
  lcd_gotoxy(0, 1);
  LCD_PUTS(" REDUCE POWER ! ");
  delay_ms_compat(AUTO_TUNE_HIGH_POWER_WARNING_MS);
}

static void showAlreadyMatched(void)
{
  lcd_clear();
  lcd_gotoxy(0, 0);
  LCD_PUTS("ALREADY MATCHED ");
  lcd_gotoxy(0, 1);
  LCD_PUTS("     OK !       ");
  delay_ms_compat(AUTO_TUNE_MATCH_MESSAGE_MS);
}

static TuneSearchResult testTuneCandidate(uint8_t l, uint8_t c, uint8_t k1, TuneBest &best);
static void activateTuneBest(const TuneBest &best);
static TuneSearchResult optimizeAxis(TuneBest &best, uint8_t axis, bool &passImproved);
static TuneSearchResult testAlternateK1(TuneBest &best, bool &passImproved);

static TuneSearchResult testTuneCandidate(uint8_t l, uint8_t c, uint8_t k1, TuneBest &best)
{
  // A fresh FWD-derived power check prevents the next relay transition.
  Samples();
  if (isAutoTunePowerTooHigh())
    return SEARCH_HIGH_POWER;
  if (!isAutoTuneRfPresent())
    return SEARCH_RF_LOST;

  L = l;
  C = c;
  sweep_relay = k1;
  k1Write(k1);
  send = LC;
  Send_LC(lCode(l), cCode(c));
  c_out(cReal(c));
  l_out(lReal(l));
  Samples();
  bargraf(0, 1, 11, Vfwd, Vrew);
  if (isAutoTunePowerTooHigh())
    return SEARCH_HIGH_POWER;
  if (!isAutoTuneRfPresent())
    return SEARCH_RF_LOST;

  const uint16_t measuredSWR = (uint16_t)swr;
  if (measuredSWR < best.swr)
  {
    best.swr = measuredSWR;
    best.l = l;
    best.c = c;
    best.k1 = k1;
    lcd_gotoxy(12, 1);
    swr = best.swr;
    swr_out();
  }
  swr = measuredSWR;

  if (!BUTTON_TUNE_SWR)
    return SEARCH_ABORTED;
  if (best.swr <= 102)
    return SEARCH_GOOD_MATCH;
  return SEARCH_CONTINUE;
}

static void activateTuneBest(const TuneBest &best)
{
  L = best.l;
  C = best.c;
  sweep_relay = best.k1;
  k1Write(best.k1);
  send = LC;
  Send_LC(lCode(best.l), cCode(best.c));
  c_out(cReal(best.c));
  l_out(lReal(best.l));
}

static void saveTuneBest(uint8_t band, uint16_t bestSWR, uint8_t bestL,
                         uint8_t bestC, uint8_t bestK1)
{
  if (bestL > MAX_IND_CAP || bestC > MAX_IND_CAP || bestK1 > 1 ||
      bestSWR < 100 || bestSWR >= MAX_SWR)
    return;

  // SWR is the validity/commit field: an interrupted write remains invalid.
  swrMinSet(band, MAX_SWR + 1UL);
  capMinSet(band, bestC);
  indMinSet(band, bestL);
  k1MinSet(band, bestK1);
  swrMinSet(band, bestSWR);
}

static TuneSearchResult optimizeAxis(TuneBest &best, uint8_t axis, bool &passImproved)
{
  TuneSearchResult result;
  const bool isInductor = axis == Ind;
  while ((isInductor ? best.l : best.c) < MAX_IND_CAP)
  {
    const uint16_t previousSWR = best.swr;
    uint8_t nextL = best.l;
    uint8_t nextC = best.c;
    if (isInductor)
      ++nextL;
    else
      ++nextC;
    result = testTuneCandidate(nextL, nextC, best.k1, best);
    if (result != SEARCH_CONTINUE)
      return result;
    if (best.swr >= previousSWR)
    {
      activateTuneBest(best);
      break;
    }
    passImproved = true;
  }

  // One compact reverse-neighbor check after the forward descent.
  if ((isInductor ? best.l : best.c) > 0)
  {
    const uint16_t previousSWR = best.swr;
    uint8_t nextL = best.l;
    uint8_t nextC = best.c;
    if (isInductor)
      --nextL;
    else
      --nextC;
    result = testTuneCandidate(nextL, nextC, best.k1, best);
    if (result != SEARCH_CONTINUE)
      return result;
    if (best.swr < previousSWR)
      passImproved = true;
    else
      activateTuneBest(best);
  }

  return SEARCH_CONTINUE;
}

static TuneSearchResult testAlternateK1(TuneBest &best, bool &passImproved)
{
  const uint16_t previousSWR = best.swr;
  TuneSearchResult result = testTuneCandidate(best.l, best.c, best.k1 ? 0 : 1, best);
  if (result != SEARCH_CONTINUE)
    return result;
  if (best.swr < previousSWR)
    passImproved = true;
  else
    activateTuneBest(best);
  return SEARCH_CONTINUE;
}

static void automatic_tune(void)
{
  lcd_gotoxy(11, 0);
  LCD_PUTS(" AUTO");
  lcd_gotoxy(12, 1);
  LCD_PUTS(">>>>");
  delay_ms_compat(200);

  Samples();
  if (isAutoTunePowerTooHigh())
  {
    showAutoTuneHighPowerWarning();
    return;
  }
  if (isAutoTuneRfPresent() && swrCompensated <= ALREADY_MATCHED_SWR)
  {
    showAlreadyMatched();
    return;
  }

  const uint8_t band = (uint8_t)bandGet();
  uint8_t startL = 0;
  uint8_t startC = 0;
  uint8_t startK1 = 0;
  uint16_t rememberedSWR = MAX_SWR;
  loadTuneMemory(band, startL, startC, startK1, rememberedSWR);

  TuneBest best = {MAX_SWR + 1, startL, startC, startK1};
  TuneSearchResult result = testTuneCandidate(startL, startC, startK1, best);
  if (result != SEARCH_CONTINUE)
    goto searchFinished;

  for (uint8_t pass = 0; pass < MAX_TUNE_PASSES; ++pass)
  {
    bool passImproved = false;
    result = optimizeAxis(best, Ind, passImproved);
    if (result != SEARCH_CONTINUE)
      goto searchFinished;
    result = optimizeAxis(best, Cap, passImproved);
    if (result != SEARCH_CONTINUE)
      goto searchFinished;
    result = testAlternateK1(best, passImproved);
    if (result != SEARCH_CONTINUE || !passImproved)
      goto searchFinished;
  }

searchFinished:
  const bool searchCompleted = result == SEARCH_CONTINUE || result == SEARCH_GOOD_MATCH;

  // Do not re-apply the best candidate if RF rose since the last test.
  if (result != SEARCH_HIGH_POWER)
  {
    Samples();
    if (isAutoTunePowerTooHigh())
      result = SEARCH_HIGH_POWER;
  }
  if (result == SEARCH_HIGH_POWER)
  {
    showAutoTuneHighPowerWarning();
    return;
  }

  // No RF-valid candidate was captured in this search.
  if (best.swr > MAX_SWR)
    return;

  // Restore the exact winning L/C/K1, including after cancel or RF loss.
  activateTuneBest(best);
  // Verification is fresh but never overwrites the tracked search minimum.
  Samples();
  bargraf(0, 1, 11, Vfwd, Vrew);
  lcd_gotoxy(12, 1);
  swr_out();

  if (isAutoTunePowerTooHigh())
  {
    showAutoTuneHighPowerWarning();
    return;
  }

  if (result == SEARCH_ABORTED)
  {
    while (!BUTTON_TUNE_SWR)
    {
    }
    return;
  }
  if (result == SEARCH_RF_LOST || !isAutoTuneRfPresent())
    return;

  if (searchCompleted)
    saveTuneBest(band, best.swr, best.l, best.c, best.k1);
}

static void restoreMainDisplay(void)
{
  lcd_clear();
  c_out(cReal(C));
  l_out(lReal(L));
  lcd_gotoxy(5, 0);
  LCD_PUTS("-");
  drawBandStatus();
}

static void armRfTune(void)
{
  if (rfTuneArmed)
    return;

  rfTuneArmed = true;
  rfTuneValidSamples = 0;
  lcd_gotoxy(11, 0);
  LCD_PUTS(" >ARM");
}

static void cancelRfTuneArm(void)
{
  if (!rfTuneArmed)
    return;

  rfTuneArmed = false;
  rfTuneValidSamples = 0;
  drawBandStatus();
}

static void runRequestedAutoTune(void)
{
  // Consume both explicit request types before entering the blocking search.
  rfTuneArmed = false;
  rfTuneValidSamples = 0;
  autoTuneState = AUTO_TUNE_ACTIVE;
  automatic_tune();
  autoTuneState = AUTO_TUNE_IDLE;
  restoreMainDisplay();
}

static void handleArmedRfTune(void)
{
  if (!rfTuneArmed || autoTuneState != AUTO_TUNE_IDLE)
    return;

  const uint16_t freshPowerW = relativePowerFromFwd((uint16_t)Vfwd);
  if (freshPowerW > AUTO_TUNE_MAX_POWER_W)
  {
    rfTuneArmed = false;
    rfTuneValidSamples = 0;
    showAutoTuneHighPowerWarning();
    restoreMainDisplay();
    return;
  }

  if (freshPowerW == 0)
  {
    rfTuneValidSamples = 0;
    return;
  }

  if (rfTuneValidSamples < RF_TUNE_VALID_SAMPLES)
    ++rfTuneValidSamples;
  if (rfTuneValidSamples >= RF_TUNE_VALID_SAMPLES)
    runRequestedAutoTune();
}

static void handleAutoTuneRequest(void)
{
  const bool buttonPressed = !BUTTON_TUNE_SWR;
  const unsigned long now = millis();

  switch (autoTuneState)
  {
  case AUTO_TUNE_IDLE:
    if (buttonPressed)
    {
      autoTuneButtonPressStart = now;
      autoTuneState = AUTO_TUNE_ARMING;
    }
    break;

  case AUTO_TUNE_ARMING:
    if (!buttonPressed)
    {
      autoTuneState = AUTO_TUNE_IDLE;
      if (rfTuneArmed)
        cancelRfTuneArm();
      else
        drawBandStatus();
    }
    else if (now - autoTuneButtonPressStart >= AUTO_TUNE_REQUEST_HOLD_MS)
    {
      // Only this deliberate button gesture may request an automatic search.
      autoTuneState = AUTO_TUNE_REQUESTED;
    }
    break;

  case AUTO_TUNE_REQUESTED:
    if (!buttonPressed)
    {
      runRequestedAutoTune();
    }
    else if (now - autoTuneButtonPressStart >= RF_TUNE_ARM_HOLD_MS)
    {
      // The 3-second gesture is consumed here; its later release does nothing.
      armRfTune();
      autoTuneState = AUTO_TUNE_RF_ARM_PRESS;
    }
    break;

  case AUTO_TUNE_RF_ARM_PRESS:
    if (!buttonPressed)
      autoTuneState = AUTO_TUNE_IDLE;
    break;

  case AUTO_TUNE_ACTIVE:
    break;
  }

  handleArmedRfTune();
}

static void manual(void)
{
  while (!BUTTON_MODE)
  {
  }
  constexpr unsigned long TARE_MESSAGE_MS = 1000UL;
  bool tareMessageActive = false;
  unsigned long tareMessageStart = 0;
  lcd_gotoxy(11, 0);
  LCD_PUTS(" MANU");
  do
  {
    uint8_t band = (uint8_t)bandGet();
    lcd_gotoxy(5, 0);
    LCD_PUTS("-");
    if (tareMessageActive && (unsigned long)(millis() - tareMessageStart) >= TARE_MESSAGE_MS)
    {
      tareMessageActive = false;
      lcd_gotoxy(11, 0);
      LCD_PUTS(" MANU");
    }
    Samples();
    bargraf(0, 1, 11, Vfwd, Vrew);
    swr_out();
    c_out(cReal(C));
    l_out(lReal(L));
    if (!BUTTON_C_UP)
    {
      C = C < 31 ? C + 1 : 0;
      send = Cap;
      Send_LC(lCode(L), cCode(C));
      capMinSet(band, C);
      delay_ms_compat(DELAY_MANUAL_TUNING);
    }
    if (!BUTTON_C_DOWN)
    {
      C = C > 0 ? C - 1 : 31;
      send = Cap;
      Send_LC(lCode(L), cCode(C));
      capMinSet(band, C);
      delay_ms_compat(DELAY_MANUAL_TUNING);
    }
    if (!BUTTON_L_UP)
    {
      L = L < 31 ? L + 1 : 0;
      send = Ind;
      Send_LC(lCode(L), cCode(C));
      indMinSet(band, L);
      delay_ms_compat(DELAY_MANUAL_TUNING);
    }
    if (!BUTTON_L_DOWN)
    {
      L = L > 0 ? L - 1 : 31;
      send = Ind;
      Send_LC(lCode(L), cCode(C));
      indMinSet(band, L);
      delay_ms_compat(DELAY_MANUAL_TUNING);
    }
    if (!BUTTON_BAND_DOWN)
    {
      L = 0;
      C = 0;
      send = LC;
      Send_LC(lCode(L), cCode(C));
      // Keep the current band's restored tuning baseline at the TARE state.
      capMinSet(band, C);
      indMinSet(band, L);
      // Invalidate the old good-match record without adding EEPROM fields.
      swrMinSet(band, MAX_SWR + 1UL);
      c_out(cReal(C));
      l_out(lReal(L));
      tareMessageActive = true;
      tareMessageStart = millis();
      lcd_gotoxy(11, 0);
      LCD_PUTS(" TARA");
      while (!BUTTON_BAND_DOWN)
      {
      }
    }
    if (!BUTTON_BAND_UP)
    {
      do
      {
        sweep_relay = sweep_relay ? 0 : 1;
      } while (BUTTON_BAND_UP);
      while (!BUTTON_BAND_UP)
      {
      }
      k1MinSet(band, sweep_relay);
      k1Write(sweep_relay);
    }
  } while (BUTTON_TUNE_SWR);
}

static void DirRef(void)
{
  while (!BUTTON_C_UP)
  {
  }
  do
  {
    Samples();
    lcd_gotoxy(0, 0);
    LCD_PUTS("Fwd");
    Formateaza(Vfwd);
    LCD_PUTS("  Rew");
    Formateaza(Vrew);
    bargraf(0, 1, 15, Vfwd, Vrew);
    delay_ms_compat(10);
  } while (BUTTON_TUNE_SWR);
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_gotoxy(0, 0);
  lcd_putse(eeText(EE_ADDR_BLANK));
  band_out();
  c_out(cReal(C));
  l_out(lReal(L));
}

static void SWR11(void)
{
  while (!BUTTON_C_DOWN)
  {
  }
  resetRelativePowerDisplay();
  resetFwdBarEnvelope();
  prepareSWRDisplay();
  do
  {
    Samples();
    updateRelativePowerDisplay((uint16_t)Vfwd);
    lcd_gotoxy(4, 0);
    swr_out();
    lcd_gotoxy(12, 0);
    sar = ON;
    Formateaza(RF_power);
    LCD_PUTS("W");
    const uint16_t fwdEnvelope = updateFwdBarEnvelope((uint16_t)Vfwd);
    // Display-only enhancement: both halves use FWD.  Vfwd/Vrew stay raw.
    bargraf(0, 1, FWD_BAR_SEGMENTS, fwdEnvelope, fwdEnvelope, false);
    delay_ms_compat(10);
  } while (BUTTON_TUNE_SWR);
  while (!BUTTON_TUNE_SWR)
  {
  }
  resetRelativePowerDisplay();
  resetFwdBarEnvelope();
  band_out();
  c_out(cReal(C));
  l_out(lReal(L));
}

static void TunnerOff(void)
{
  while (!BUTTON_L_UP)
  {
  }
  lcd_gotoxy(0, 0);
  lcd_putse(eeText(EE_ADDR_STRING4));
  send = LC;
  Send_LC(0, 0);
  do
  {
    Samples();
    bargraf(0, 1, 15, Vfwd, Vrew);
    lcd_gotoxy(12, 0);
    swr_out();
    delay_ms_compat(10);
  } while (BUTTON_TUNE_SWR);
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_gotoxy(0, 0);
  lcd_putse(eeText(EE_ADDR_BLANK));
  band_out();
  tunner();
}

static void DelayRelay(void)
{
  while (!BUTTON_L_DOWN)
  {
  }
  lcd_gotoxy(0, 0);
  lcd_putse(eeText(EE_ADDR_STRING3));
  // Remove the previous screen's bargraph once; do not redraw this row later.
  lcd_gotoxy(0, 1);
  LCD_PUTS("                ");
  uint16_t lastDisplayedDelay = 0xFFFF;
  do
  {
    uint16_t delayBeforeAdc = eeWord(EE_ADDR_DELAY_BEFORE_ADC);
    if (!BUTTON_BAND_DOWN)
    {
      delayBeforeAdc -= 5;
      if (delayBeforeAdc < 5)
        delayBeforeAdc = 5;
      eeWordSet(EE_ADDR_DELAY_BEFORE_ADC, delayBeforeAdc);
    }
    delay_ms_compat(150);
    delayBeforeAdc = eeWord(EE_ADDR_DELAY_BEFORE_ADC);
    if (!BUTTON_BAND_UP)
    {
      delayBeforeAdc += 5;
      if (delayBeforeAdc > 50)
        delayBeforeAdc = 50;
      eeWordSet(EE_ADDR_DELAY_BEFORE_ADC, delayBeforeAdc);
    }
    if (delayBeforeAdc != lastDisplayedDelay)
    {
      lcd_gotoxy(4, 1);
      sar = false;
      Formateaza(delayBeforeAdc);
      LCD_PUTS(" ms");
      lastDisplayedDelay = delayBeforeAdc;
    }
  } while (BUTTON_TUNE_SWR);
  while (!BUTTON_TUNE_SWR)
  {
  }
  lcd_gotoxy(0, 0);
  lcd_putse(eeText(EE_ADDR_BLANK));
  band_out();
}

void setup(void)
{
  hardwareInit();
  constexpr uint8_t STARTUP_VREF_INVALID = 0x01;
  constexpr uint8_t STARTUP_COUPLER_INVALID = 0x02;
  constexpr uint8_t STARTUP_POWER_INVALID = 0x04;
  constexpr uint8_t STARTUP_TUNER_STATE_INVALID = 0x08;
  volatile uint8_t startupDiagnosticFlags = 0;

  const uint16_t savedVref = eeWord(EE_ADDR_UREF);
  activeVrefMv = isValidVref(savedVref) ? savedVref : VREF_FACTORY_MV;
  if (!isValidVref(savedVref))
    startupDiagnosticFlags |= STARTUP_VREF_INVALID;
  if (!initializeCouplerCalibration())
    startupDiagnosticFlags |= STARTUP_COUPLER_INVALID;
  if (!initializeRelativePowerCalibration())
    startupDiagnosticFlags |= STARTUP_POWER_INVALID;
  if (!isValidStoredTunerState())
    startupDiagnosticFlags |= STARTUP_TUNER_STATE_INVALID;

  // Diagnostics complete before the splash; no user-facing self-test screen.
  lcd_init(16);
  lcd_clear();
  lcd_putse(eeText(EE_ADDR_STRING1));
  lcd_putse(eeText(EE_ADDR_STRING2));
  delay_ms_compat(1000);
  if (!BUTTON_TUNE_SWR)
    setari();
  lcd_clear();
  loadNormalBarGlyphs();
  band_out();
  tunner();
}

void loop(void)
{
  Samples();
  bargraf(0, 1, 11, Vfwd, Vrew);
  swr_out();
  handleAutoTuneRequest();
  if (!BUTTON_BAND_UP)
  {
    uint16_t b = bandGet();
    bandSet(b < 10 ? b + 1 : 0);
    band_out();
    while (!BUTTON_BAND_UP)
    {
    }
  }
  if (!BUTTON_BAND_DOWN)
  {
    uint16_t b = bandGet();
    bandSet(b > 0 ? b - 1 : 10);
    band_out();
    while (!BUTTON_BAND_DOWN)
    {
    }
  }
  if (!BUTTON_C_UP)
  {
    delay_ms_compat(50);
    DirRef();
  }
  if (!BUTTON_C_DOWN)
  {
    delay_ms_compat(50);
    SWR11();
  }
  if (!BUTTON_L_UP)
  {
    delay_ms_compat(50);
    TunnerOff();
  }
  if (!BUTTON_L_DOWN)
  {
    delay_ms_compat(50);
    DelayRelay();
  }
  if (!BUTTON_MODE)
  {
    delay_ms_compat(50);
    manual();
  }
}
