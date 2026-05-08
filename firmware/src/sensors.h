#pragma once
#include <cstdint>

struct SensorData {
    float    temp_C   = 0;
    float    humidity = 0;
    uint16_t co2_ppm  = 0;
    float    lux      = 0;
    float    spl_dBA  = 0;
    bool     temp_ok  = false;
    bool     hum_ok   = false;
    bool     co2_ok   = false;
    bool     lux_ok   = false;
    bool     spl_ok   = false;
};

bool sensorsBegin();
void sensorsPoll(SensorData& out);
