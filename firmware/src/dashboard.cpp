#include "dashboard.h"
#include "nvs_cfg.h"
#include "config.h"
#include <WiFi.h>
#include <Arduino.h>
#include <stdio.h>

Dashboard dashboard;

// ─── Dashboard HTML template ──────────────────────────
// Thresholds are injected as JS constants at serve time.
// Sensor values are fetched live every 5 s from /api/data.

static const char DASH_HEAD[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RoomIQ</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#07101f;color:#e2e8f0;font-family:-apple-system,BlinkMacSystemFont,sans-serif;
     padding:1.5rem;min-height:100vh}
header{display:flex;justify-content:space-between;align-items:center;
       padding-bottom:1rem;margin-bottom:1.5rem;
       border-bottom:1px solid rgba(255,255,255,.07)}
h1{font-size:1.5rem;font-weight:800;color:#f0b429}
.ts{font-size:.78rem;color:#64748b}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:1rem}
.card{background:#0d1a2e;border:1px solid rgba(255,255,255,.07);border-radius:12px;
      padding:1.4rem;border-top:3px solid #334155;transition:border-top-color .4s}
.card.ok  {border-top-color:#4ade80}
.card.warn{border-top-color:#f0b429}
.card.bad {border-top-color:#f87171}
.param{font-size:.68rem;font-weight:700;text-transform:uppercase;letter-spacing:.09em;
       color:#64748b;margin-bottom:.5rem}
.value{font-size:2rem;font-weight:800;line-height:1;margin-bottom:.2rem;
       transition:color .4s}
.card.ok  .value{color:#4ade80}
.card.warn .value{color:#f0b429}
.card.bad  .value{color:#f87171}
.unit{font-size:.78rem;color:#64748b}
.label{font-size:.75rem;margin-top:.55rem}
.card.ok   .label{color:#4ade80}
.card.warn .label{color:#f0b429}
.card.bad  .label{color:#f87171}
footer{margin-top:2rem;display:flex;justify-content:space-between;align-items:center;
       font-size:.78rem;color:#334155}
.reset{color:#334155;cursor:pointer;background:none;border:none;font-size:.78rem}
.reset:hover{color:#f87171}
</style></head>
<body>
<header>
  <h1>RoomIQ</h1>
  <div class="ts" id="ts">Connecting…</div>
</header>
<div class="grid" id="grid">
  <div class="card"><div class="param">Temperature</div><div class="value" id="v-temp">—</div><div class="unit">°C</div><div class="label" id="l-temp"></div></div>
  <div class="card" id="c-hum"><div class="param">Humidity</div><div class="value" id="v-hum">—</div><div class="unit">% RH</div><div class="label" id="l-hum"></div></div>
  <div class="card" id="c-co2"><div class="param">CO&#8322;</div><div class="value" id="v-co2">—</div><div class="unit">ppm</div><div class="label" id="l-co2"></div></div>
  <div class="card" id="c-lux"><div class="param">Light</div><div class="value" id="v-lux">—</div><div class="unit">lux</div><div class="label" id="l-lux"></div></div>
  <div class="card" id="c-spl"><div class="param">Noise</div><div class="value" id="v-spl">—</div><div class="unit">dBA</div><div class="label" id="l-spl"></div></div>
</div>
<footer>
  <span>)html";

// After DASH_HEAD, we inject:  device IP + thresholds as JS vars
// Then DASH_TAIL closes the page.

static const char DASH_TAIL[] PROGMEM = R"html(
</span>
  <button class="reset" onclick="resetDevice()">⚙ Reset setup</button>
</footer>
<script>
function classify(v, lo, hi, invert) {
  if (v === null) return '';
  if (invert) { // lower is better (CO2, noise)
    if (v <= lo)  return 'ok';
    if (v <= hi)  return 'warn';
    return 'bad';
  } else {       // range (temp, humidity, lux)
    if (v >= lo && v <= hi) return 'ok';
    if (v < lo - (hi-lo)*0.15 || v > hi + (hi-lo)*0.15) return 'bad';
    return 'warn';
  }
}

const LABELS = {ok:'Within range', warn:'Borderline', bad:'Out of range'};

function set(id, val, cls, unit) {
  const card = document.getElementById('c-' + id) || document.querySelector('.card');
  const cardEl = document.getElementById('c-' + id);
  if (cardEl) { cardEl.className = 'card ' + cls; }
  const v = document.getElementById('v-' + id);
  const l = document.getElementById('l-' + id);
  if (v) v.textContent = val !== null ? (Number.isInteger(val) ? val : val.toFixed(1)) : '—';
  if (l) l.textContent = cls ? LABELS[cls] : '';
}

function update() {
  fetch('/api/data').then(r => r.json()).then(d => {
    document.getElementById('ts').textContent = 'Updated ' + new Date().toLocaleTimeString();

    const tc = document.getElementById('c-temp') || document.querySelectorAll('.card')[0];
    const tempCard = document.querySelectorAll('.card')[0];
    const tempCls = classify(d.temp, T_MIN, T_MAX, false);
    if (tempCard) tempCard.className = 'card ' + tempCls;
    const vt = document.getElementById('v-temp');
    const lt = document.getElementById('l-temp');
    if (vt) vt.textContent = d.temp !== null ? d.temp.toFixed(1) : '—';
    if (lt) lt.textContent = LABELS[tempCls] || '';

    set('hum', d.hum,  classify(d.hum,  H_MIN, H_MAX, false));
    set('co2', d.co2,  classify(d.co2,  CO2_MAX, CO2_MAX * 1.3, true));
    set('lux', d.lux,  classify(d.lux,  LUX_MIN, LUX_MIN * 3,   false));
    set('spl', d.spl,  classify(d.spl,  SPL_MAX, SPL_MAX + 10,  true));
  }).catch(() => {
    document.getElementById('ts').textContent = 'Connection error — retrying…';
  });
}

function resetDevice() {
  if (!confirm('Clear all settings and re-run setup?')) return;
  fetch('/reset', {method:'POST'}).then(() => {
    document.getElementById('ts').textContent = 'Resetting…';
  });
}

setInterval(update, 5000);
update();
</script>
</body></html>
)html";

// ─── Implementation ───────────────────────────────────

bool Dashboard::begin(const RoomConfig& cfg) {
    _cfg = cfg;

    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

    Serial.printf("[dashboard] connecting to \"%s\"", cfg.wifi_ssid);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - t0 > WIFI_TIMEOUT_MS) {
            Serial.println("\n[dashboard] WiFi timeout");
            return false;
        }
        delay(250);
        Serial.print('.');
    }
    Serial.printf("\n[dashboard] IP %s\n", WiFi.localIP().toString().c_str());

    _http.on("/",         HTTP_GET,  [this]() { serveRoot(); });
    _http.on("/api/data", HTTP_GET,  [this]() { serveData(); });
    _http.on("/reset",    HTTP_POST, [this]() { handleReset(); });
    _http.begin();

    return true;
}

void Dashboard::handle() {
    _http.handleClient();
}

void Dashboard::push(const SensorData& data) {
    _latest = data;
}

void Dashboard::serveRoot() {
    // Build page: inject device IP string + threshold JS constants between HEAD and TAIL
    String page;
    page.reserve(4096);
    page += FPSTR(DASH_HEAD);

    // Inject IP text visible in footer
    page += WiFi.localIP().toString();

    char js[256];
    snprintf(js, sizeof(js),
             "</span>"
             "<script>"
             "const T_MIN=%.1f,T_MAX=%.1f,"
             "H_MIN=%.1f,H_MAX=%.1f,"
             "CO2_MAX=%u,LUX_MIN=%.1f,SPL_MAX=%.1f;"
             "</script>",
             _cfg.temp_min, _cfg.temp_max,
             _cfg.hum_min,  _cfg.hum_max,
             _cfg.co2_max,
             _cfg.lux_min,
             _cfg.spl_max);
    page += js;
    page += FPSTR(DASH_TAIL);

    _http.send(200, "text/html", page);
}

void Dashboard::serveData() {
    // Null values for sensors that haven't returned a valid reading yet
    char json[256];
    snprintf(json, sizeof(json),
             "{"
             "\"temp\":%s,"
             "\"hum\":%s,"
             "\"co2\":%s,"
             "\"lux\":%s,"
             "\"spl\":%.1f"
             "}",
             _latest.temp_ok ? String(_latest.temp_C,   1).c_str() : "null",
             _latest.hum_ok  ? String(_latest.humidity, 1).c_str() : "null",
             _latest.co2_ok  ? String(_latest.co2_ppm).c_str()     : "null",
             _latest.lux_ok  ? String(_latest.lux,      1).c_str() : "null",
             _latest.spl_dBA);

    _http.sendHeader("Cache-Control", "no-cache");
    _http.send(200, "application/json", json);
}

void Dashboard::handleReset() {
    _http.send(200, "text/plain", "Resetting…");
    delay(500);
    nvsCfg.clear();
    ESP.restart();
}
