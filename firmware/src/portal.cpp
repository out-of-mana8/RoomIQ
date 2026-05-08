#include "portal.h"
#include "nvs_cfg.h"
#include "config.h"
#include <WiFi.h>
#include <Arduino.h>
#include <string.h>

CaptivePortal portal;

// ─── Setup form HTML ──────────────────────────────────
static const char FORM_HTML[] PROGMEM = R"html(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RoomIQ Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#07101f;color:#e2e8f0;font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:2rem 1rem;min-height:100vh}
.card{background:#0d1a2e;border:1px solid rgba(255,255,255,.07);border-radius:12px;max-width:480px;margin:0 auto;padding:2rem}
h1{font-size:1.8rem;font-weight:800;color:#f0b429;margin-bottom:.2rem}
.sub{color:#64748b;font-size:.88rem;margin-bottom:2rem}
h2{font-size:.7rem;font-weight:700;letter-spacing:.1em;text-transform:uppercase;color:#f0b429;
   margin:1.75rem 0 .75rem;padding-bottom:.4rem;border-bottom:1px solid rgba(255,255,255,.07)}
label{display:block;font-size:.8rem;color:#94a3b8;margin-bottom:.3rem}
input{width:100%;padding:.6rem .9rem;background:#112039;border:1px solid rgba(255,255,255,.1);
      border-radius:8px;color:#e2e8f0;font-size:.95rem;margin-bottom:.9rem;outline:none;transition:border-color .2s}
input:focus{border-color:rgba(240,180,41,.5)}
.row{display:grid;grid-template-columns:1fr 1fr;gap:.75rem}
.row input{margin-bottom:0}
.row-wrap{margin-bottom:.9rem}
button{width:100%;padding:.9rem;background:#f0b429;color:#07101f;border:none;border-radius:8px;
       font-size:1rem;font-weight:700;cursor:pointer;margin-top:.5rem;transition:background .2s}
button:hover{background:#d4960f}
</style></head>
<body><div class="card">
<h1>RoomIQ</h1>
<p class="sub">Wi-Fi credentials and comfort thresholds are stored in device flash and persist across reboots.</p>
<form method="POST" action="/save">

  <h2>Wi-Fi</h2>
  <label>Network (SSID)</label>
  <input name="ssid" required autocomplete="off" placeholder="Your network name">
  <label>Password</label>
  <input name="pass" type="password" autocomplete="off" placeholder="Leave blank if open">

  <h2>Temperature</h2>
  <div class="row row-wrap">
    <div><label>Min (°C)</label><input name="t_min" type="number" step=".5" value="20"></div>
    <div><label>Max (°C)</label><input name="t_max" type="number" step=".5" value="26"></div>
  </div>

  <h2>Humidity</h2>
  <div class="row row-wrap">
    <div><label>Min (% RH)</label><input name="h_min" type="number" value="30"></div>
    <div><label>Max (% RH)</label><input name="h_max" type="number" value="70"></div>
  </div>

  <h2>CO&#8322; / Light / Noise</h2>
  <label>CO&#8322; Max (ppm) — WELL v2: 800 ppm</label>
  <input name="co2_max" type="number" value="1000">
  <label>Light Min (lux) — WELL v2: 300 lux</label>
  <input name="lux_min" type="number" value="300">
  <label>Noise Max (dBA) — WELL v2: 35 dBA</label>
  <input name="spl_max" type="number" value="45">

  <button type="submit">Save &amp; Connect</button>
</form>
</div></body></html>
)html";

static const char SAVED_HTML[] PROGMEM = R"html(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RoomIQ – Saved</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#07101f;color:#e2e8f0;font-family:-apple-system,sans-serif;
     display:flex;align-items:center;justify-content:center;min-height:100vh;padding:2rem}
.card{background:#0d1a2e;border:1px solid rgba(255,255,255,.07);border-radius:12px;
      padding:2.5rem;text-align:center;max-width:360px}
.icon{font-size:2.5rem;margin-bottom:1rem}
h1{font-size:1.4rem;font-weight:700;color:#4ade80;margin-bottom:.5rem}
p{color:#64748b;font-size:.9rem;line-height:1.6}
</style></head>
<body><div class="card">
<div class="icon">✓</div>
<h1>Saved</h1>
<p>Connecting to your network…<br>RoomIQ will restart in 2 seconds.<br><br>
   Find its IP address in your router's device list,<br>then open it in your browser.</p>
</div></body></html>
)html";

// ─── Implementation ───────────────────────────────────

void CaptivePortal::begin() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    delay(500);  // give the AP time to come up

    // DNS: resolve every hostname → AP IP so the captive portal pops up
    _dns.start(53, "*", WiFi.softAPIP());

    _http.on("/",     HTTP_GET,  [this]() { serveForm(); });
    _http.on("/save", HTTP_POST, [this]() { handleSave(); });
    _http.onNotFound( [this]()  { handleNotFound(); });
    _http.begin();

    Serial.printf("[portal] AP \"%s\"  IP %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
}

void CaptivePortal::handle() {
    _dns.processNextRequest();
    _http.handleClient();
}

void CaptivePortal::serveForm() {
    _http.send_P(200, "text/html", FORM_HTML);
}

void CaptivePortal::handleSave() {
    if (!_http.hasArg("ssid") || _http.arg("ssid").isEmpty()) {
        _http.send(400, "text/plain", "SSID is required.");
        return;
    }

    RoomConfig cfg{};
    strlcpy(cfg.wifi_ssid, _http.arg("ssid").c_str(), sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_pass, _http.arg("pass").c_str(), sizeof(cfg.wifi_pass));
    cfg.temp_min = _http.arg("t_min").toFloat();
    cfg.temp_max = _http.arg("t_max").toFloat();
    cfg.hum_min  = _http.arg("h_min").toFloat();
    cfg.hum_max  = _http.arg("h_max").toFloat();
    cfg.co2_max  = (uint16_t)_http.arg("co2_max").toInt();
    cfg.lux_min  = _http.arg("lux_min").toFloat();
    cfg.spl_max  = _http.arg("spl_max").toFloat();

    nvsCfg.save(cfg);
    Serial.printf("[portal] saved SSID \"%s\"\n", cfg.wifi_ssid);

    _http.send_P(200, "text/html", SAVED_HTML);
    delay(2000);
    ESP.restart();
}

void CaptivePortal::handleNotFound() {
    // Android / iOS captive-portal detection: redirect everything to the form
    _http.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    _http.send(302, "text/plain", "");
}
