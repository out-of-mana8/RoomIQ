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

The firmware runs a five-state machine: **BOOT → SENSE → DISPLAY → SLEEP**, with **CONFIG** mode entered on first boot or via a long-press of BTN_C. In CONFIG, the ESP32 brings up a soft access point and serves a captive portal for setting Wi-Fi credentials and comfort thresholds, which are written to NVS flash.

```
                Power-On / EN Reset
                       │
                       ▼
                ┌─────────────┐
                │    BOOT     │  Init GPIO, SPI, I²C, RMT, LEDC
                │             │  Mount SD · Read NVS
                └──────┬──────┘
               valid   │   empty
                 ┌─────┘     └──────────────┐
                 │                          ▼
                 │                   ┌─────────────┐
                 │                   │   CONFIG    │  softAP + captive portal
                 │                   │             │  Write creds + thresholds
                 │                   └──────┬──────┘
                 └──────────────────────────┘
                                     │
                                     ▼
                              ┌─────────────┐  BTN_A  ┌─────────────┐
                              │    SENSE    │─────────►│   DISPLAY   │
                              │  Poll 5 s   │◄─────────│  3 views    │
                              │  Log 60 s   │  30s idle│             │
                              └─────────────┘          └──────┬──────┘
                                                              │ BTN_B
                                                              ▼
                                                       ┌─────────────┐
                                                       │    SLEEP    │
                                                       │  60 s rate  │
                                                       │  Light-sleep│
                                                       └─────────────┘
```

In SENSE, the polling loop runs every 5 seconds. Digital I²C sensors are read synchronously; the microphone uses a background-filled DMA buffer that firmware retrieves and applies digital A-weighting to compute dBA. Display and LED are updated immediately. SD logging and Wi-Fi JSON push are decoupled to every 60 seconds — averaged over the 12 intervening readings — to protect the LDO's thermal budget from frequent Wi-Fi TX spikes and to reduce SD flash write cycles.

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
├── images/
│   ├── 3D_Render_Front.png
│   ├── 3D_Render_Back.png
│   ├── PCB_Top_Layer.png
│   └── PCB_Bottom_Layer.png
└── hardware/
    ├── Schematic.pdf
    ├── Drawings.pdf
    ├── Gerber.zip          ← JLCPCB-ready, order directly
    └── BOM.xlsx
```

**Component datasheets:** [ESP32-C6](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf) · [SCD40](https://sensirion.com/media/documents/48C4B7FB/64C134E7/Sensirion_SCD4x_Datasheet.pdf) · [SHT41](https://sensirion.com/media/documents/33C09C07/622B9FC5/Datasheet_SHT4x.pdf) · [OPT3001](https://www.ti.com/lit/ds/symlink/opt3001.pdf) · [MCP6001](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf) · [WS2812B-4020](https://cdn-shop.adafruit.com/product-files/4684/4684_WS2812B-4020.pdf) · [AP7361C](https://www.diodes.com/assets/Datasheets/AP7361C.pdf)

---

*Designed in EasyEDA · Fabricated at JLCPCB*
