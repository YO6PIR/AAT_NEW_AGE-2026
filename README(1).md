# AAT New Age — Automatic Antenna Tuner

**AAT New Age** is a modernized automatic antenna tuner firmware for a compact HF matching unit built around an **ATmega328 (non‑P)**, an **HD44780-compatible 16×2 LCD**, relay-switched L/C networks, and an RF directional coupler.

The project started from an older, fully functional tuner and was progressively ported, reorganized, calibrated, and upgraded while keeping the existing hardware philosophy intact. The final firmware focuses on **fast tuning, predictable relay control, accurate SWR indication, practical relative power measurement, useful band memory, and a simple front-panel workflow**.

> **Design goal:** a practical, fast and reliable antenna tuner for real amateur-radio use — not a laboratory instrument.

---

## Highlights

- ATmega328 **non‑P** at **8 MHz internal RC oscillator**
- No external crystal; PB6/PB7 remain available as I/O
- Arduino/MiniCore-friendly firmware
- HD44780 16×2 LCD
- Automatic and manual tuning
- Relay-switched inductance and capacitance networks
- User-adjustable relay settling delay
- Compensated SWR measurement using a calibrated directional coupler
- Six-point coupler leakage calibration
- Factory coupler calibration fallback
- **Relative Power** calculated directly from FWD RAW
- Two-point Relative Power calibration at 10 W / 100 W
- No AD8307 required by the final power-display architecture
- Fast SWR/PWR bargraph with FWD envelope behavior
- Manual **TARE** function
- Per-band **Memory Tune**
- **Already Matched** detection before AUTO TUNE
- Robust **Best SWR** tracking and restoration
- High-power protection for AUTO TUNE
- One-shot RF-triggered tuning arm mode: `>ARM`
- Persistent VREF calibration
- EEPROM-backed user settings and calibration data
- `.hex` and `.eep` build artifacts for direct programming

---

# 1. Hardware platform

## MCU

The final target is:

- **ATmega328, non‑P**
- **8 MHz internal oscillator**
- no external crystal
- no bootloader required for the intended programming workflow

Running from the internal oscillator is intentional. It keeps the existing hardware unchanged and leaves the crystal pins available for I/O.

The project has enough Flash, SRAM and EEPROM headroom on the ATmega328 for the final feature set.

## Display

- HD44780-compatible **16×2 character LCD**
- 4-bit interface
- custom CGRAM characters used for the bargraph

The LCD remains deliberately simple. A large part of the UI work focused on obtaining useful visual feedback without introducing unnecessary flicker or expensive redraws.

## RF measurements

The tuner uses a directional/tandem coupler that provides:

- **FWD RAW**
- **REV RAW**

These two ADC values are the core RF measurements used by the final firmware.

FWD is also used as the source for the **Relative Power** indication.

## Relay network

The tuner switches inductance and capacitance through serial relay-control chains and also supports the existing topology selection hardware.

Relay timing remains user-adjustable from the front panel.

---

# 2. Why “New Age”?

The original tuner was already functional, but the modernization had several goals:

1. move away from the old development environment;
2. port the firmware to an Arduino-compatible toolchain;
3. use the available ATmega328 resources more effectively;
4. keep the existing hardware;
5. improve SWR accuracy;
6. make AUTO TUNE faster and safer;
7. modernize calibration and EEPROM handling;
8. improve the LCD user experience;
9. add useful memory and protection logic without making operation complicated.

The result is not a complete rewrite of the tuner concept. It is a careful modernization of a proven hardware platform.

---

# 3. Firmware organization

The original monolithic firmware was reorganized into functional modules without changing the hardware architecture.

Typical modules include:

- `AAT_New_Age.ino` — application flow / UI state
- `Config.h` — constants and project configuration
- `Hardware.*` — board-level hardware access
- `Measurements.*` — ADC/RF measurements
- `SWR.*` — SWR computation and compensation
- `PowerMeter.*` — Relative Power processing
- `Storage.*` — EEPROM calibration/settings
- `AppState.*` — application state

Low-level direct AVR access is retained only where deterministic relay shifting benefits from it. Higher-level GPIO, ADC, EEPROM, timing and LCD behavior use Arduino-compatible APIs where practical.

---

# 4. SWR measurement

## Raw measurements

The firmware acquires:

- `FWD_RAW`
- `REV_RAW`

The raw values remain untouched for diagnostics and control logic.

## Why compensation was necessary

On a real 50 Ω dummy load the directional coupler has finite directivity. A small REV signal therefore exists even when the load is correctly matched.

Without compensation, the tuner displayed an artificial SWR offset.

The final firmware does **not** alter the raw ADC values. Instead it calculates the expected leakage of the coupler and derives a compensated REV value for SWR computation.

This keeps:

- RAW diagnostics honest
- RF detection independent
- SWR compensation isolated to the SWR path

## Factory coupler reference

The built-in default leakage curve is:

| FWD RAW | expected REV RAW |
|---:|---:|
| 136 | 3 |
| 224 | 11 |
| 297 | 19 |
| 429 | 35 |
| 657 | 66 |
| 851 | 92 |

Interpolation between these points provides a practical correction of the real coupler characteristic.

After calibration/compensation, a good 50 Ω dummy load typically reads approximately **SWR 1.01–1.02** over the useful power range.

---

# 5. Coupler Calibration

The tuner includes a user-accessible six-point coupler calibration.

Calibration mode is entered by holding **ENTER during startup**.

The operator applies six increasing RF levels and captures the corresponding FWD/REV RAW pairs.

Important design choices:

- calibration uses **RAW values**, not guessed watts;
- FWD calibration points must be strictly increasing;
- points are held in RAM until final confirmation;
- EEPROM is written only after SAVE;
- cancelling preserves the previous calibration;
- invalid calibration automatically falls back to the factory curve.

A **Restore Factory Defaults** option restores the built-in coupler curve.

---

# 6. VREF Calibration

The ADC reference value can be adjusted from the calibration menu.

Controls:

- `Band +` → increase
- `Band -` → decrease
- `ENTER` → save/confirm

The calibration screen remembers the last saved VREF value from EEPROM.

Therefore:

- first use / invalid EEPROM → factory fallback, typically **5000 mV**
- after saving a new value → the next calibration session starts from that saved value

The VREF screen is updated only when the value changes, eliminating unnecessary LCD flicker.

---

# 7. Relative Power — no AD8307 required

One of the largest final simplifications is the power-meter architecture.

Earlier versions used a dedicated **AD8307 logarithmic detector** on ADC0.

The final design defines the displayed power explicitly as:

> **RELATIVE POWER**

It is derived directly from the same **FWD RAW** signal already available from the directional coupler.

This provides several advantages:

- no separate logarithmic detector is required by the final firmware;
- no separate ADC0 power-measurement path is needed;
- power indication follows the RF envelope more naturally;
- SSB response is more immediate;
- the implementation is simpler and better integrated with the tuner.

This is deliberately a **relative/practical watt indication**, not a precision laboratory wattmeter.

---

# 8. Relative Power Calibration

Relative Power uses a two-point calibration:

- **10 W**
- **100 W**

During calibration the firmware captures **FWD RAW**, not AD8307 output.

Factory/reference defaults:

| Reference | FWD RAW |
|---:|---:|
| 10 W | 300 |
| 100 W | 867 |

These values are based on the actual tuner/coupler hardware and can be replaced by user calibration.

The displayed intermediate power is an estimate referenced to those two calibration points.

The intent is repeatable, useful field indication rather than traceable absolute power measurement.

---

# 9. SWR / PWR display

The SWR/PWR screen displays:

- numeric SWR
- numeric Relative Power
- RF bargraph

The final bargraph reuses the original fine-resolution dual-glyph renderer, but in SWR/PWR display mode both visual halves are driven by the same **FWD envelope**.

In other words:

```text
TOP    = FWD
BOTTOM = FWD
```

This produces a visually full bar while preserving the fine scaling of the original renderer.

The former `F/R` label is unnecessary in this mode, so the complete 16-character line can be used by the bargraph.

## Bargraph envelope

The bargraph uses:

- very fast attack
- slower controlled fall

The validated decay uses a **3× RAW decay step**, producing a much more natural response than a one-unit-per-refresh fall.

The bar itself therefore provides an intuitive peak-envelope effect without requiring a separate moving peak marker.

---

# 10. Manual Tune

MANUAL mode allows direct control of the tuner network.

The operator can adjust:

- inductance
- capacitance
- existing topology state as supported by the hardware

Manual switching continues to use the configured relay delay.

---

# 11. TARE

Inside MANUAL mode, `Band -` acts as a **TARE / tuning reset** command.

TARE:

- sets L = 0
- sets C = 0
- applies the zero state to the relay network
- makes this zero condition the new active tuning baseline
- prevents the previously remembered tuning state from being immediately restored

For user feedback, the MANUAL status briefly changes:

```text
MANU → TARA → MANU
```

`TARA` is displayed for approximately one second using non-blocking timing.

The purpose is simple: when the operator deliberately zeroes the tuner, the firmware respects that decision.

---

# 12. AUTO TUNE

AUTO TUNE searches the available L/C/topology combinations and evaluates SWR to find the best valid match.

The relay-settling delay is not hardcoded to a single value. It remains adjustable by the user through **Delay Time Relay**.

Typical selectable values are in the range:

```text
5, 10, 15 ... 50 ms
```

On the tested hardware, **15 ms** provides very fast operation while remaining reliable.

---

# 13. Memory Tune per band

The tuner remembers the last successful tuning solution for each band.

A remembered solution includes the relevant tuning state, such as:

- L
- C
- topology/K1 where applicable
- best SWR information where used by the implementation

When returning to a previously tuned band, the firmware can start from the known good state instead of blindly beginning from zero.

This substantially reduces unnecessary searching.

TARE intentionally invalidates/bypasses the old current tuning baseline until a new successful tune establishes a new memory.

---

# 14. Already Matched detection

Before starting a full AUTO TUNE search, the firmware performs a fresh SWR check.

If the current state already provides approximately:

```text
SWR ≤ 1.30
```

the tuner does not perform unnecessary relay switching.

Instead it keeps the current L/C state and reports a short match/OK indication.

Benefits:

- faster operation
- fewer relay cycles
- less mechanical wear
- no pointless retuning of an already good match

The threshold is defined as a named constant so it can be adjusted easily if desired.

---

# 15. Best SWR robustness

During AUTO TUNE the firmware tracks the complete best solution, not merely the latest candidate.

A new candidate becomes BEST only when its SWR is genuinely lower than the current best.

The stored winning state includes:

- best SWR
- best L
- best C
- best topology/K1

Equal-SWR candidates do not continuously replace an already valid winner.

At normal completion the tuner explicitly restores the **best physical relay state**, rather than accidentally remaining on the final candidate that happened to be tested.

Invalid/no-RF measurements cannot become the best result.

This prevents a later worse candidate from corrupting a better match discovered earlier in the search.

---

# 16. High Power protection

AUTO TUNE includes an RF power interlock.

The purpose is to avoid switching the tuner relay network at unnecessarily high RF power.

The initial safety threshold is approximately:

```text
15 W Relative Power
```

Before AUTO begins, and during the search, the firmware verifies that RF power is within the safe tuning range.

If excessive power is detected:

- tuning does not begin, or is aborted safely;
- no further candidate relay switching is performed;
- the LCD requests reduced power;
- tuning does not automatically restart when power falls.

The user must explicitly request tuning again.

---

# 17. Explicit AUTO commands and RF arm mode

The tuner must never start searching merely because RF appears.

AUTO TUNE requires explicit user intent.

The main button supports distinct press-duration actions:

### Short press

Acts as `ESC` and returns to the main screen.

### Normal deliberate hold/release

Starts the standard user-requested AUTO TUNE.

### Hold for approximately 3 seconds

Arms one-shot RF-triggered tuning.

The display shows:

```text
>ARM
```

The tuner then waits for RF.

At the first valid RF event:

1. RF validity is checked;
2. high-power protection is applied;
3. Already Matched logic is applied;
4. if tuning is required and safe, AUTO TUNE runs once;
5. the arm state clears.

The arm is **one-shot**. A later transmission cannot retrigger AUTO until the operator deliberately arms it again.

---

# 18. No accidental tuning

Normal transmission without `>ARM`:

- updates SWR
- updates Relative Power
- updates the bargraph
- **does not start AUTO TUNE**

MANUAL mode also never launches AUTO simply because RF is detected.

This explicit-state approach avoids dangerous or confusing relay activity during ordinary transmission.

---

# 19. EEPROM behavior

EEPROM stores calibration and operating data such as:

- VREF
- coupler calibration
- Relative Power calibration
- tuning memory / band state
- relay timing and other persistent settings

Calibration validity is checked before use.

Where practical, invalid EEPROM data falls back to safe factory defaults rather than blocking startup.

The build can also generate a separate `.eep` image for direct EEPROM programming.

---

# 20. Build target

The project is intended for an **ATmega328 at 8 MHz using the internal oscillator**.

A MiniCore-style target is recommended.

Example configuration concept:

```text
ATmega328
8 MHz internal
no bootloader
EEPROM retained
LTO enabled
```

The common Arduino Uno target is not appropriate because it assumes a 16 MHz external resonator and does not match the intended use of the oscillator pins as I/O.

---

# 21. Programming

The project can be built to produce:

- firmware `.hex`
- EEPROM `.eep`

These files can be programmed directly with `avrdude` or another AVR ISP workflow.

A bootloader is not required.

---

# 22. Recommended fuse philosophy

For the final hardware:

- internal 8 MHz RC oscillator
- no external crystal
- brown-out protection appropriate for the supply
- no bootloader if programming directly by ISP

Fuse values must always be checked against the exact ATmega328 device/core/programmer configuration before programming.

---

# 23. Calibration workflow summary

A typical complete calibration sequence is:

1. Hold **ENTER** during startup to enter calibration.
2. Adjust/save **VREF**.
3. Calibrate **Relative Power**:
   - SET 10 W → capture FWD RAW
   - SET 100 W → capture FWD RAW
4. Calibrate the directional coupler with six increasing RF points if desired.
5. SAVE.
6. Verify SWR on a good 50 Ω dummy load.

Factory defaults remain available for recovery.

---

# 24. Typical operating workflow

## Normal monitoring

Transmit normally.

The tuner displays SWR, Relative Power and the FWD envelope without moving relays.

## Immediate AUTO TUNE

Apply low RF power and issue the normal TUNE command.

The tuner:

1. checks safe power;
2. checks whether the antenna is already matched;
3. uses remembered band state where applicable;
4. searches only when necessary;
5. tracks the best valid SWR;
6. restores the winning L/C state.

## RF-armed tuning

Hold the main button for about three seconds until:

```text
>ARM
```

Then apply RF.

The tuner performs one automatic tuning cycle when RF is detected and then disarms.

## Manual reset

Enter MANUAL and press `Band -`.

The tuner performs:

```text
TARA
```

and sets L/C to zero.

---

# 25. Design philosophy

Several decisions define the final project.

### Keep the hardware

The modernization was deliberately software-focused. The existing tuner hardware remains useful and does not need to be redesigned simply to obtain better behavior.

### Keep raw measurements raw

FWD/REV RAW values are not silently altered. Compensation is applied only where it belongs.

### Calibrate the real hardware

The directional coupler is treated as a real component with finite directivity and its own leakage curve.

### Relative is enough where absolute is unnecessary

The AAT does not pretend to be a precision RF wattmeter. Relative Power is more than sufficient for safe tuning and practical operation.

### Avoid needless relay movement

Memory Tune and Already Matched reduce search time and mechanical wear.

### Safety before cleverness

High-power protection and explicit AUTO state prevent unwanted relay switching.

### Simple UI

The 16×2 LCD and a small number of buttons remain enough for fast day-to-day operation.

---

# 26. Final project status

The project has progressed from an older monolithic tuner firmware to a substantially modernized and hardware-validated controller with:

- cleaner source organization
- ATmega328 / Arduino-compatible build
- improved tuning speed
- calibrated SWR compensation
- user coupler calibration
- Relative Power from FWD
- improved bargraph behavior
- persistent VREF
- Manual TARE
- Memory Tune
- Already Matched detection
- Best SWR result protection
- high-power AUTO interlock
- explicit RF AUTO arm mode

At this point **AAT New Age is considered functionally complete**.

Further changes should be treated as optional experiments rather than requirements for normal use.

---

# 27. Notes

This project is intended for amateur-radio experimentation and station use.

RF systems can expose equipment to high voltage, current and RF energy. Relay ratings, coupler construction, grounding, RF layout and safe tuning power remain hardware responsibilities.

Always validate the tuner first on a suitable **50 Ω dummy load** before using it with an antenna.

---

## Author / project

**AAT New Age — Automatic Antenna Tuner**

Modernization, firmware development and hardware validation performed as an amateur-radio project.

**YO6PIR**
