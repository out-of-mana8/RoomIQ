#pragma once
#include <Preferences.h>
#include <cstdint>

struct RoomConfig {
    char     wifi_ssid[64];
    char     wifi_pass[64];
    float    temp_min;
    float    temp_max;
    float    hum_min;
    float    hum_max;
    uint16_t co2_max;
    float    lux_min;
    float    spl_max;
};

class NvsCfg {
public:
    bool isConfigured();
    bool load(RoomConfig& cfg);
    void save(const RoomConfig& cfg);
    void clear();

private:
    Preferences _prefs;
};

extern NvsCfg nvsCfg;
