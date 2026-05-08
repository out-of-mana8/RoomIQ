#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "nvs_cfg.h"
#include "sensors.h"
#include "portal.h"
#include "dashboard.h"

enum AppMode { MODE_CONFIG, MODE_NORMAL };
static AppMode appMode;

static RoomConfig cfg;
static SensorData sensorData;
static uint32_t   lastSenseMs  = 0;
static uint32_t   btnCPressedAt = 0;
static bool       btnCWasDown   = false;

// ─── setup ────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[RoomIQ] booting");

    // Buttons
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);

    // Sensors — non-fatal: we still run the web UI even if a sensor is absent
    sensorsBegin();

    if (!nvsCfg.isConfigured()) {
        Serial.println("[boot] no config → captive portal");
        portal.begin();
        appMode = MODE_CONFIG;
        return;
    }

    nvsCfg.load(cfg);
    Serial.printf("[boot] loaded config: SSID=\"%s\"\n", cfg.wifi_ssid);

    if (!dashboard.begin(cfg)) {
        // WiFi failed — wipe creds so next boot goes straight to setup
        Serial.println("[boot] WiFi failed — clearing NVS, restarting");
        nvsCfg.clear();
        delay(1000);
        ESP.restart();
    }

    // Kick off the first sensor read immediately
    sensorsPoll(sensorData);
    dashboard.push(sensorData);
    lastSenseMs = millis();

    appMode = MODE_NORMAL;
    Serial.println("[boot] running — open http://" + WiFi.localIP().toString());
}

// ─── loop ─────────────────────────────────────────────

void loop() {
    if (appMode == MODE_CONFIG) {
        portal.handle();
        return;
    }

    // Normal mode ──────────────────────────────────────

    // Poll sensors on interval
    if (millis() - lastSenseMs >= SENSE_INTERVAL_MS) {
        lastSenseMs = millis();
        sensorsPoll(sensorData);
        dashboard.push(sensorData);
        Serial.printf("[sense] T=%.1f°C  H=%.1f%%  CO2=%u ppm  Lux=%.0f  SPL=%.1f dBA\n",
                      sensorData.temp_C, sensorData.humidity,
                      sensorData.co2_ppm, sensorData.lux, sensorData.spl_dBA);
    }

    // Handle HTTP clients
    dashboard.handle();

    // BTN_C long-press → wipe NVS and re-run setup
    bool btnCDown = (digitalRead(PIN_BTN_C) == LOW);
    if (btnCDown && !btnCWasDown) {
        btnCPressedAt = millis();
    }
    if (btnCDown && (millis() - btnCPressedAt >= BTN_LONGPRESS_MS)) {
        Serial.println("[main] BTN_C long-press → clearing NVS, restarting");
        nvsCfg.clear();
        delay(500);
        ESP.restart();
    }
    btnCWasDown = btnCDown;
}
