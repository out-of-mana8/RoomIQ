#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <SensirionI2cScd4x.h>
#include <Arduino.h>
#include <math.h>

static Adafruit_SHT4x    sht4x;
static SensirionI2cScd4x scd4x;

// ─── OPT3001 (direct Wire) ────────────────────────────

static void opt3001Write(uint8_t reg, uint16_t val) {
    Wire.beginTransmission(OPT3001_ADDR);
    Wire.write(reg);
    Wire.write(val >> 8);
    Wire.write(val & 0xFF);
    Wire.endTransmission();
}

static uint16_t opt3001Read(uint8_t reg) {
    Wire.beginTransmission(OPT3001_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)OPT3001_ADDR, (uint8_t)2);
    return ((uint16_t)Wire.read() << 8) | Wire.read();
}

static float opt3001Lux() {
    uint16_t raw = opt3001Read(OPT3001_REG_RESULT);
    uint8_t  exp = (raw >> 12) & 0x0F;
    uint16_t man = raw & 0x0FFF;
    return 0.01f * (float)(1 << exp) * (float)man;
}

// ─── Microphone SPL ───────────────────────────────────
// Chain: S15OT421 (−42 dBV/Pa) → Av 100× → ADC_11db (2.5 V range, 12-bit)
// At 94 dBSPL (1 Pa): V_out = 1 × 10^(−42/20) × 100 ≈ 0.794 Vrms
// In 12-bit counts (2.5 V FS): 0.794/2.5 × 4095 ≈ 1302 counts
// REF_COUNTS needs physical calibration — adjust until SPL meter agrees.
static constexpr float MIC_REF_COUNTS = 1302.0f;

static float readSPL() {
    // Pass 1: compute DC offset to remove mic bias
    long sum = 0;
    for (int i = 0; i < MIC_SAMPLES; i++) sum += analogRead(PIN_MIC);
    int dc = (int)(sum / MIC_SAMPLES);

    // Pass 2: RMS of AC component
    long sumSq = 0;
    for (int i = 0; i < MIC_SAMPLES; i++) {
        long s = (long)analogRead(PIN_MIC) - dc;
        sumSq += s * s;
    }
    float rms = sqrtf((float)sumSq / MIC_SAMPLES);
    if (rms < 1.0f) rms = 1.0f;

    return 94.0f + 20.0f * log10f(rms / MIC_REF_COUNTS);
}

// ─── Init ─────────────────────────────────────────────

bool sensorsBegin() {
    Wire.begin(PIN_SDA, PIN_SCL);

    // SHT41
    if (!sht4x.begin()) {
        Serial.println("[sensors] SHT41 not found");
        return false;
    }
    sht4x.setPrecision(SHT4X_HIGH_PRECISION);
    sht4x.setHeater(SHT4X_NO_HEATER);

    // SCD40
    scd4x.begin(Wire, SCD40_ADDR);
    scd4x.stopPeriodicMeasurement();   // stop any lingering session
    delay(500);
    scd4x.startPeriodicMeasurement();  // first reading ready after ~5 s

    // OPT3001 — continuous conversion, auto-range, 800 ms
    opt3001Write(OPT3001_REG_CONFIG, OPT3001_CFG_CONT);

    // ADC for mic
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.println("[sensors] all initialised");
    return true;
}

// ─── Poll ─────────────────────────────────────────────

void sensorsPoll(SensorData& out) {
    // SHT41
    sensors_event_t ev_hum, ev_temp;
    out.temp_ok = sht4x.getEvent(&ev_hum, &ev_temp);
    out.hum_ok  = out.temp_ok;
    if (out.temp_ok) {
        out.temp_C   = ev_temp.temperature;
        out.humidity = ev_hum.relative_humidity;
    }

    // SCD40 — read only when a fresh measurement is ready
    uint16_t dataReady = 0;
    scd4x.getDataReadyStatus(dataReady);
    out.co2_ok = (dataReady & 0x07FF) != 0;
    if (out.co2_ok) {
        float scdT, scdH;
        scd4x.readMeasurement(out.co2_ppm, scdT, scdH);
    }

    // OPT3001
    out.lux    = opt3001Lux();
    out.lux_ok = (out.lux >= 0.0f);

    // Mic
    out.spl_dBA = readSPL();
    out.spl_ok  = true;
}
