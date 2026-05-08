# RoomIQ — Indoor Environmental Quality Monitor

A compact, self-contained mixed-signal PCB that simultaneously measures five indoor environmental parameters and evaluates them against ASHRAE 55-2023 and the WELL Building Standard v2.

**Platform:** ESP32-C6 (RISC-V @ 160 MHz, Wi-Fi 6, BLE 5.3) &nbsp;|&nbsp; **Board:** 2-layer, 96.5 × 63.5 mm &nbsp;|&nbsp; **EDA:** EasyEDA &nbsp;|&nbsp; **Fab:** JLCPCB

<p align="center">
  <img src="images/3D_Render_Front.png" width="100%">
</p>

---

## What It Does

Most consumer indoor monitors address a single metric — a CO₂ sensor here, a thermostat there. RoomIQ fuses five sensor channels into one device: temperature, humidity, CO₂, ambient light, and acoustic noise. Readings are evaluated against two tiers of thresholds — user-configurable limits set via a Wi-Fi captive portal, and published standards — to distinguish occupant discomfort (amber) from genuinely unsafe conditions (red alert) without crying wolf.

The design is motivated by three well-documented but commonly ignored problems: CO₂ above 1,000 ppm measurably impairs decision-making within 1–2 hours in an unventilated room; ASHRAE 55-2023 defines thermal comfort in terms of *both* temperature and humidity, not just temperature alone; and ambient light and noise levels are almost never monitored despite strong links to eye strain, sleep disruption, and elevated cortisol.

Results display on a 2.0" IPS TFT with adaptive backlight, a side-emitting WS2812B RGB status LED, and a local Wi-Fi dashboard fed by JSON push over Wi-Fi 6. Configuration and thresholds persist in NVS flash. Readings are logged to an SD card in CSV format for offline analysis.

---

## Board

<p align="center">
  <img src="images/3D_Render_Front.png" width="49%">
  <img src="images/3D_Render_Back.png" width="49%">
</p>

<p align="center">
  <img src="images/PCB_Top_Layer.png" width="49%">
  <img src="images/PCB_Bottom_Layer.png" width="49%">
</p>

The board is partitioned into five functional zones. Power (USB-C → ESD → AP7361C LDO → 3.3 V) occupies the lower-left with the shortest possible path to the ESP32 VDD pins. The WROOM module sits center with a 3 mm copper keep-out under its antenna. Digital sensors cluster upper-left near the I²C pull-ups, with the SCD40 CO₂ sensor facing open air and the SHT41 placed more than 1 cm away to avoid thermal coupling during measurement peaks. The analog mic front-end is physically isolated in the lower-right zone with GND star-tied back to the digital ground near the LDO — zero digital trace crossings. The TFT header runs along the right edge with short, parallel SPI traces.

The bottom layer is a solid GND copper pour that acts as an RF and analog shield across the full board area. Full layer drawings are in [`hardware/Drawings.pdf`](hardware/Drawings.pdf).

---

## Sensors

| Parameter | Sensor | Accuracy / Range | Standard |
|-----------|--------|------------------|----------|
| Temperature & Humidity | Sensirion SHT41 | ±0.2 °C / ±1.8 % RH | ASHRAE 55-2023 |
| CO₂ | Sensirion SCD40 | ±50 ppm + 5 % (400–2000 ppm) | WELL v2 < 800 ppm |
| Ambient Light | TI OPT3001 | 0.01–83,865 lux, 23-bit | WELL v2 ≥ 300 lux |
| Acoustic Noise | S15OT421 + MCP6001 | 59 dB SNR, 130 dBSPL AOP | WELL v2 < 35 dBA |

All three digital sensors share a single I²C bus (IO10/IO11, 100 kHz, 4.7 kΩ pull-ups) at non-conflicting addresses: SHT41 at 0x44, SCD40 at 0x62, OPT3001 at 0x45. The SCD40 sets the clock limit; the other two support 400 kHz Fast Mode and are unaffected.

---

## Analog Microphone Signal Chain

The acoustic path is the most involved part of the design. A MEMS capsule's raw output needs DC blocking, gain, anti-aliasing, and careful biasing before the ADC can make use of it.

```
[S15OT421 MEMS mic]  →  [HPF: 10µF + 1kΩ, fc ≈ 16 Hz]  →  [MCP6001 inverting amp, Av = 100×]
                     →  [AAF: 390Ω + 100nF, fc ≈ 4.08 kHz]  →  [ADC1_CH0, ATTEN2, 0–1.9 V]
```

The MCP6001 runs in an inverting configuration with a gain of 100× (+40 dB). The non-inverting input is biased at 1.65 V (VDD/2 via a 100k/100k divider) so the output sits centered in the ADC's linear range. A 100 pF feedback capacitor rolls off at 15.9 kHz to prevent the op-amp from oscillating into the AAF. The anti-aliasing RC sits between the op-amp output and the ADC input to isolate the op-amp's output impedance from the ADC's switched-capacitor input.

At a reference level of 60 dBSPL, the mic produces roughly 16 mV RMS after gain — swinging the ADC between about 1.55 V and 1.75 V, well within ATTEN2's 0–1.9 V range. Firmware flags near-rail samples to detect clipping during louder events.

---

## Schematic

Full schematic exported from EasyEDA: [`hardware/Schematic.pdf`](hardware/Schematic.pdf)

Key design decisions captured in the schematic:
- **WS2812B level shift** — The ESP32-C6 VOH at light load (~3.0–3.1 V) falls below the WS2812B's 3.5 V VIH threshold. A BSS138 N-MOSFET level-shifts IO8 to 5 V VBUS to meet spec.
- **SCD40 single-supply** — VDD and VDDH are tied to 3.3 V on the PCB per Sensirion's single-supply guidance.
- **USB-C CC resistors** — 5.1 kΩ to GND on CC1 and CC2 advertise 5 V / 0.9 A sink; 27 Ω series on D+/D− for USB 2.0 stub termination.
- **Button debounce** — 10 kΩ pull-up + 100 nF RC (τ = 1 ms) on each of the three user buttons and the reset line.

---

## Firmware

Built with PlatformIO (Arduino framework). Source lives in [`firmware/src/`](firmware/src/).

### Boot Flow

```
Power-On / EN Reset
        │
        ▼
   Read NVS flash
        │
   configured?
   ┌────┴────┐
  no        yes
   │         │
   ▼         ▼
CONFIG     connect to saved WiFi
           (20 s timeout → wipe NVS + restart)
  softAP        │
  "RoomIQ-      ▼
   Setup"    NORMAL MODE
  DNS →      HTTP server on port 80
  192.168    │
  .4.1       ├─ GET  /          → live IEQ dashboard
             ├─ GET  /api/data  → JSON sensor readings
             └─ POST /reset     → wipe NVS + restart
```

**CONFIG mode** — The device raises a `RoomIQ-Setup` access point (no password). DNS redirects every hostname to 192.168.4.1 so the captive portal pops up automatically on phones. The setup form collects Wi-Fi SSID + password and five comfort thresholds, saves them to NVS flash, then restarts.

**Normal mode** — Sensors are polled every 5 seconds. The web dashboard at the device's IP auto-refreshes with colour-coded cards (green / amber / red) evaluated against the stored thresholds. Hold BTN_C for 3 seconds to wipe NVS and re-enter setup.

### Getting Started

**Dependencies** (PlatformIO installs automatically from `platformio.ini`):
- `adafruit/Adafruit SHT4x Library`
- `sensirion/Sensirion I2C SCD4x`

```bash
# Clone and open in VS Code with PlatformIO extension
git clone https://github.com/out-of-mana8/RoomIQ
cd RoomIQ/firmware

# Build + flash (board auto-detected via USB-JTAG)
pio run --target upload

# Monitor serial output
pio device monitor
```

**First boot:**
1. Connect the board via USB-C — serial output confirms boot state
2. Join the `RoomIQ-Setup` Wi-Fi network on your phone
3. The setup page opens automatically; enter your Wi-Fi credentials and thresholds
4. The board restarts, connects to your network, and prints its IP address over serial
5. Open that IP in a browser

**SPL calibration:** `MIC_REF_COUNTS` in [`firmware/src/sensors.cpp`](firmware/src/sensors.cpp) is pre-calculated from the signal chain math (1302 counts ≈ 94 dBSPL). Tune this value against a calibrated SPL meter for accurate readings on your specific board.

---

## Alert Thresholds

Three-tier logic: user-configured limits → ASHRAE 55-2023 → WELL Building Standard v2.

| Parameter | ASHRAE 55-2023 | WELL v2 | Alert |
|-----------|---------------|---------|-------|
| Temperature | 20–26 °C (operative) | 20–25 °C | Amber banner + yellow LED |
| Humidity | 30–70 % RH | 30–60 % RH | Amber banner + yellow LED |
| CO₂ | 1000 ppm (proxy) | < 800 ppm | Red LED + force wake from SLEEP |
| Illuminance | — (informational) | ≥ 300 lux task area | Amber display highlight |
| Noise (SPL) | < 45 dBA office | < 35 dBA background | Yellow LED + log event |

---

## Power

The AP7361C-33E LDO (SOT-223) takes 5 V from USB-C VBUS and delivers 3.3 V at up to 800 mA. The WS2812B status LED runs directly off VBUS to keep the LDO load down. At average load (~200 mA) the LDO dissipates about 0.34 W — a 37 °C junction rise, well within the 125 °C maximum. At peak load (~495 mA, simultaneous Wi-Fi TX + SCD40 measurement + TFT backlight) dissipation climbs to 0.84 W and ΔTJ reaches ~92 °C, which makes a copper heat-spreader pad on the LDO's exposed tab necessary for sustained operation.

---

## Bill of Materials

Single-unit cost is approximately **$58–65 USD**. The two dominant line items are the Sensirion SCD40 CO₂ sensor ($14.90) and the Adafruit 2.0" TFT display ($14.95); everything else is commodity. Full spreadsheet: [`hardware/BOM.xlsx`](hardware/BOM.xlsx)

---

## Known Limitations

- **SPL is approximate** — A-weighting is applied in firmware, not hardware. A proper multi-pole analog filter would improve accuracy.
- **ADC headroom at loud SPL** — the 100× gain stage is tuned for quiet-to-moderate indoor levels. Louder transients can clip; a future revision should reduce gain or switch to a wider ADC attenuation range.
- **CO₂ long-term drift** — the SCD40 needs periodic recalibration. Automatic self-calibration (ASC) mode and a forced-recal routine are on the roadmap.
- **No barometric pressure input** — the SCD40 can correct for altitude if pressure is supplied; a BMP388 on the same I²C bus would handle this.
- **Raw audio privacy** — audio is never buffered beyond the RMS computation window and is never transmitted over Wi-Fi.

---

## Repository

```
RoomIQ/
├── README.md
├── index.html              ← GitHub Pages portfolio site
├── images/
│   ├── 3D_Render_Front.png
│   ├── 3D_Render_Back.png
│   ├── PCB_Top_Layer.png
│   └── PCB_Bottom_Layer.png
├── hardware/
│   ├── Schematic.pdf
│   ├── Drawings.pdf
│   ├── Gerber.zip          ← JLCPCB-ready, order directly
│   └── BOM.xlsx
└── firmware/
    ├── platformio.ini
    └── src/
        ├── main.cpp        ← setup() / loop() / state machine
        ├── config.h        ← pin definitions, timing constants
        ├── nvs_cfg.h/.cpp  ← NVS Preferences wrapper
        ├── sensors.h/.cpp  ← SHT41, SCD40, OPT3001, mic ADC
        ├── portal.h/.cpp   ← SoftAP + DNS + captive portal form
        └── dashboard.h/.cpp← WiFi connect + HTTP IEQ dashboard
```

**Component datasheets:** [ESP32-C6](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf) · [SCD40](https://sensirion.com/media/documents/48C4B7FB/64C134E7/Sensirion_SCD4x_Datasheet.pdf) · [SHT41](https://sensirion.com/media/documents/33C09C07/622B9FC5/Datasheet_SHT4x.pdf) · [OPT3001](https://www.ti.com/lit/ds/symlink/opt3001.pdf) · [MCP6001](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf) · [WS2812B-4020](https://cdn-shop.adafruit.com/product-files/4684/4684_WS2812B-4020.pdf) · [AP7361C](https://www.diodes.com/assets/Datasheets/AP7361C.pdf)

---

*Designed in EasyEDA · Fabricated at JLCPCB*
