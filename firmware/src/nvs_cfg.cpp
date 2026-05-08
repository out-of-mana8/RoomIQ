#include "nvs_cfg.h"
#include "config.h"
#include <cstring>

NvsCfg nvsCfg;

bool NvsCfg::isConfigured() {
    _prefs.begin(NVS_NS, true);
    bool ok = _prefs.getBool("configured", false);
    _prefs.end();
    return ok;
}

bool NvsCfg::load(RoomConfig& cfg) {
    _prefs.begin(NVS_NS, true);
    if (!_prefs.getBool("configured", false)) {
        _prefs.end();
        return false;
    }
    strlcpy(cfg.wifi_ssid, _prefs.getString("wifi_ssid", "").c_str(), sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_pass, _prefs.getString("wifi_pass", "").c_str(), sizeof(cfg.wifi_pass));
    cfg.temp_min = _prefs.getFloat("t_min",    20.0f);
    cfg.temp_max = _prefs.getFloat("t_max",    26.0f);
    cfg.hum_min  = _prefs.getFloat("h_min",    30.0f);
    cfg.hum_max  = _prefs.getFloat("h_max",    70.0f);
    cfg.co2_max  = _prefs.getUShort("co2_max", 1000);
    cfg.lux_min  = _prefs.getFloat("lux_min",  300.0f);
    cfg.spl_max  = _prefs.getFloat("spl_max",  45.0f);
    _prefs.end();
    return true;
}

void NvsCfg::save(const RoomConfig& cfg) {
    _prefs.begin(NVS_NS, false);
    _prefs.putString("wifi_ssid", cfg.wifi_ssid);
    _prefs.putString("wifi_pass", cfg.wifi_pass);
    _prefs.putFloat("t_min",     cfg.temp_min);
    _prefs.putFloat("t_max",     cfg.temp_max);
    _prefs.putFloat("h_min",     cfg.hum_min);
    _prefs.putFloat("h_max",     cfg.hum_max);
    _prefs.putUShort("co2_max",  cfg.co2_max);
    _prefs.putFloat("lux_min",   cfg.lux_min);
    _prefs.putFloat("spl_max",   cfg.spl_max);
    _prefs.putBool("configured", true);
    _prefs.end();
}

void NvsCfg::clear() {
    _prefs.begin(NVS_NS, false);
    _prefs.clear();
    _prefs.end();
}
