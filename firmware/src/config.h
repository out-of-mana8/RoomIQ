#pragma once
#include <cstdint>

// ─── I²C ──────────────────────────────────────────────
static constexpr int PIN_SDA = 10;
static constexpr int PIN_SCL = 11;

// ─── Analog mic ───────────────────────────────────────
static constexpr int  PIN_MIC     = 0;
static constexpr int  MIC_SAMPLES = 512;

// ─── SPI / Display ────────────────────────────────────
static constexpr int PIN_TFT_CS = 4;
static constexpr int PIN_TFT_DC = 5;
static constexpr int PIN_SCK    = 6;
static constexpr int PIN_MOSI   = 7;
static constexpr int PIN_MISO   = 2;
static constexpr int PIN_TFT_BL = 1;
static constexpr int PIN_SD_CS  = 3;

// ─── RGB LED ──────────────────────────────────────────
static constexpr int PIN_LED = 8;

// ─── Buttons ──────────────────────────────────────────
static constexpr int PIN_BTN_A = 18;
static constexpr int PIN_BTN_B = 19;
static constexpr int PIN_BTN_C = 21;   // long-press → re-enter CONFIG

// ─── OPT3001 ──────────────────────────────────────────
static constexpr uint8_t  OPT3001_ADDR       = 0x45;
static constexpr uint8_t  OPT3001_REG_RESULT = 0x00;
static constexpr uint8_t  OPT3001_REG_CONFIG = 0x01;
// Auto-range | 800 ms conversion time | continuous mode
static constexpr uint16_t OPT3001_CFG_CONT   = 0xCE00;

// ─── SCD40 ────────────────────────────────────────────
static constexpr uint8_t SCD40_ADDR = 0x62;

// ─── NVS namespace ────────────────────────────────────
static constexpr const char* NVS_NS = "roomiq";

// ─── Timing ───────────────────────────────────────────
static constexpr uint32_t SENSE_INTERVAL_MS  = 5000;
static constexpr uint32_t WIFI_TIMEOUT_MS    = 20000;
static constexpr uint32_t BTN_LONGPRESS_MS   = 3000;

// ─── Soft AP ──────────────────────────────────────────
static constexpr const char* AP_SSID = "RoomIQ-Setup";
