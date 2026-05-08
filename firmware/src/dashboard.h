#pragma once
#include <WebServer.h>
#include "nvs_cfg.h"
#include "sensors.h"

class Dashboard {
public:
    bool begin(const RoomConfig& cfg);
    void handle();
    void push(const SensorData& data);  // called after each sensor poll

private:
    WebServer  _http{80};
    RoomConfig _cfg{};
    SensorData _latest{};

    void serveRoot();
    void serveData();   // GET /api/data  → JSON
    void handleReset(); // POST /reset    → clear NVS + restart
};

extern Dashboard dashboard;
