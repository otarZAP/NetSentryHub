#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "config.h"
#include "tls_cert.h"
#include "session_mgr.h"
#include "data_store.h"
#include "display_mgr.h"

static AsyncWebServer server(WEB_PORT);

// ─── Embedded HTML pages ──────────────────────────────────────────────────
// Stored as PROGMEM to save RAM. Served directly without SPIFFS.

static const char LOGIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NetSentryHub — Login</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0a0a;color:#e0e0e0;font-family:monospace;
     display:flex;align-items:center;justify-content:center;min-height:100vh}
.card{background:#111;border:1px solid #333;padding:2rem;width:320px;border-radius:4px}
h1{color:#00ff88;font-size:1.2rem;margin-bottom:0.3rem}
p{color:#666;font-size:0.75rem;margin-bottom:1.5rem}
label{display:block;font-size:0.8rem;color:#aaa;margin-bottom:0.3rem}
input{width:100%;background:#1a1a1a;border:1px solid #333;color:#e0e0e0;
      padding:0.6rem;font-family:monospace;font-size:0.9rem;border-radius:2px;margin-bottom:1rem}
input:focus{outline:none;border-color:#00ff88}
button{width:100%;background:#00ff88;color:#000;border:none;padding:0.7rem;
       font-family:monospace;font-size:0.9rem;font-weight:bold;
       border-radius:2px;cursor:pointer}
button:hover{background:#00cc66}
.err{color:#ff4444;font-size:0.8rem;margin-top:0.5rem;text-align:center}
</style></head><body>
<div class="card">
  <h1>&#9632; NetSentryHub</h1>
  <p>WiFi IDS Aggregator — v)rawliteral" DEVICE_VERSION R"rawliteral(</p>
  <form method="POST" action="/login">
    <label>Username</label>
    <input type="text" name="user" autocomplete="username" autofocus>
    <label>Password</label>
    <input type="password" name="pass" autocomplete="current-password">
    <button type="submit">AUTHENTICATE</button>
    %ERR%
  </form>
</div>
</body></html>
)rawliteral";

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NetSentryHub — Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0a0a;color:#e0e0e0;font-family:monospace;font-size:0.85rem}
header{background:#111;border-bottom:1px solid #00ff88;padding:0.6rem 1rem;
       display:flex;justify-content:space-between;align-items:center}
header h1{color:#00ff88;font-size:1rem}
header nav a{color:#aaa;text-decoration:none;margin-left:1rem;font-size:0.8rem}
header nav a:hover{color:#00ff88}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:1rem;padding:1rem}
.stat{background:#111;border:1px solid #222;padding:1rem;border-radius:4px;text-align:center}
.stat .val{font-size:2rem;font-weight:bold;color:#00ff88}
.stat .lbl{color:#666;font-size:0.75rem;margin-top:0.2rem}
.stat.warn .val{color:#ffaa00}
.stat.crit .val{color:#ff4444}
section{padding:0 1rem 1rem}
h2{color:#aaa;font-size:0.8rem;border-bottom:1px solid #222;
   padding-bottom:0.3rem;margin-bottom:0.5rem;text-transform:uppercase}
table{width:100%;border-collapse:collapse;font-size:0.8rem}
th{color:#666;text-align:left;padding:0.3rem 0.5rem;border-bottom:1px solid #222}
td{padding:0.3rem 0.5rem;border-bottom:1px solid #111}
tr:hover td{background:#151515}
.badge{display:inline-block;padding:0.1rem 0.4rem;border-radius:2px;font-size:0.7rem}
.open{background:#ff4444;color:#000}.wep{background:#ff8800;color:#000}
.wpa{background:#ffcc00;color:#000}.wpa2{background:#4488ff;color:#fff}
.wpa3{background:#00ff88;color:#000}
.crit{background:#ff4444;color:#000}.high{background:#ff8800;color:#000}
.med{background:#ffcc00;color:#000}.low{background:#888;color:#000}
.ok{color:#00ff88}.offline{color:#ff4444}
.btn{background:#222;border:1px solid #333;color:#aaa;padding:0.2rem 0.6rem;
     font-family:monospace;font-size:0.75rem;cursor:pointer;border-radius:2px}
.btn:hover{border-color:#00ff88;color:#00ff88}
.btn-danger{border-color:#ff4444;color:#ff4444}
.tabs{display:flex;gap:0.5rem;margin-bottom:0.5rem}
.tab{background:#111;border:1px solid #222;color:#666;padding:0.3rem 0.8rem;
     cursor:pointer;font-family:monospace;font-size:0.8rem;border-radius:2px}
.tab.active{border-color:#00ff88;color:#00ff88}
.tab-content{display:none}.tab-content.active{display:block}
#toast{position:fixed;bottom:1rem;right:1rem;background:#00ff88;color:#000;
       padding:0.5rem 1rem;border-radius:4px;display:none;font-size:0.8rem}
</style></head><body>
<header>
  <h1>&#9632; NetSentryHub <span id="role" style="color:#666;font-size:0.75rem"></span></h1>
  <nav>
    <a href="#" onclick="loadTab('networks')">Networks</a>
    <a href="#" onclick="loadTab('probes')">Probes</a>
    <a href="#" onclick="loadTab('alerts')">Alerts</a>
    <a href="#" onclick="loadTab('devices')">Nodes</a>
    <a href="/api/report.json" target="_blank">Report JSON</a>
    <a href="/api/report.csv">Report CSV</a>
    <a href="/logout">Logout</a>
  </nav>
</header>

<div class="grid" id="stats">
  <div class="stat"><div class="val" id="s-nets">-</div><div class="lbl">Networks</div></div>
  <div class="stat warn" id="s-open-card"><div class="val" id="s-open">-</div><div class="lbl">OPEN</div></div>
  <div class="stat"><div class="val" id="s-probes">-</div><div class="lbl">Probes</div></div>
  <div class="stat crit" id="s-alerts-card"><div class="val" id="s-alerts">-</div><div class="lbl">Active Alerts</div></div>
  <div class="stat"><div class="val" id="s-devices">-</div><div class="lbl">Nodes</div></div>
</div>

<section>
  <div class="tabs">
    <div class="tab active" id="tab-networks" onclick="loadTab('networks')">Networks</div>
    <div class="tab" id="tab-probes"   onclick="loadTab('probes')">Probes</div>
    <div class="tab" id="tab-alerts"   onclick="loadTab('alerts')">Alerts</div>
    <div class="tab" id="tab-devices"  onclick="loadTab('devices')">Nodes</div>
  </div>

  <div class="tab-content active" id="pane-networks">
    <table><thead><tr><th>SSID</th><th>BSSID</th><th>Vendor</th><th>Enc</th><th>Ch</th><th>RSSI</th><th>Node</th></tr></thead>
    <tbody id="tbl-networks"><tr><td colspan="7" style="color:#444">Loading...</td></tr></tbody></table>
  </div>

  <div class="tab-content" id="pane-probes">
    <table><thead><tr><th>Source MAC</th><th>Vendor</th><th>Seeking</th><th>RSSI</th><th>Node</th></tr></thead>
    <tbody id="tbl-probes"><tr><td colspan="5" style="color:#444">Loading...</td></tr></tbody></table>
  </div>

  <div class="tab-content" id="pane-alerts">
    <table><thead><tr><th>Type</th><th>Severity</th><th>Detail</th><th>Node</th><th>Action</th></tr></thead>
    <tbody id="tbl-alerts"><tr><td colspan="5" style="color:#444">Loading...</td></tr></tbody></table>
  </div>

  <div class="tab-content" id="pane-devices">
    <table><thead><tr><th>Node ID</th><th>IP</th><th>Last Seen</th><th>Events</th><th>Status</th></tr></thead>
    <tbody id="tbl-devices"><tr><td colspan="5" style="color:#444">Loading...</td></tr></tbody></table>
  </div>
</section>

<div id="toast"></div>

<script>
let currentTab = 'networks';

function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.style.display='block';
  setTimeout(()=>t.style.display='none', 2500);
}

function encBadge(e) {
  const cls = {OPEN:'open',WEP:'wep',WPA:'wpa',WPA2:'wpa2',WPA3:'wpa3'}[e]||'';
  return `<span class="badge ${cls}">${e}</span>`;
}
function sevBadge(s) {
  const cls = {CRIT:'crit',HIGH:'high',MED:'med',LOW:'low'}[s]||'';
  return `<span class="badge ${cls}">${s}</span>`;
}

async function loadSummary() {
  try {
    const r = await fetch('/api/summary');
    const d = await r.json();
    document.getElementById('s-nets').textContent    = d.networks;
    document.getElementById('s-open').textContent    = d.open_networks;
    document.getElementById('s-probes').textContent  = d.probes;
    document.getElementById('s-alerts').textContent  = d.active_alerts;
    document.getElementById('s-devices').textContent = d.devices;
  } catch(e) {}
}

async function loadTab(tab) {
  currentTab = tab;
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.tab-content').forEach(t=>t.classList.remove('active'));
  document.getElementById('tab-'+tab).classList.add('active');
  document.getElementById('pane-'+tab).classList.add('active');

  if (tab === 'networks') {
    const r = await fetch('/api/networks'); const d = await r.json();
    document.getElementById('tbl-networks').innerHTML = d.map(n=>
      `<tr><td>${n.ssid||'[hidden]'}</td><td style="color:#666">${n.bssid}</td>
       <td>${n.vendor}</td><td>${encBadge(n.enc)}${n.wps?' <span class="badge med">WPS</span>':''}</td>
       <td>${n.channel}</td><td>${n.rssi}</td><td style="color:#666">${n.device_id}</td></tr>`
    ).join('') || '<tr><td colspan="7" style="color:#444">None yet</td></tr>';
  }
  else if (tab === 'probes') {
    const r = await fetch('/api/probes'); const d = await r.json();
    document.getElementById('tbl-probes').innerHTML = d.map(p=>
      `<tr><td style="color:#666">${p.src}</td><td>${p.vendor}</td>
       <td>${p.seeking||'&lt;wildcard&gt;'}</td><td>${p.rssi}</td>
       <td style="color:#666">${p.device_id}</td></tr>`
    ).join('') || '<tr><td colspan="5" style="color:#444">None yet</td></tr>';
  }
  else if (tab === 'alerts') {
    const r = await fetch('/api/alerts'); const d = await r.json();
    document.getElementById('tbl-alerts').innerHTML = d.map(a=>
      `<tr style="${a.acked?'opacity:0.4':''}">
       <td>${a.type}</td><td>${sevBadge(a.severity)}</td>
       <td>${a.detail}</td><td style="color:#666">${a.device_id}</td>
       <td>${a.acked?'<span style="color:#444">acked</span>':
           `<button class="btn" onclick="ackAlert(${a.idx})">ACK</button>`}</td></tr>`
    ).join('') || '<tr><td colspan="5" style="color:#444">No alerts</td></tr>';
  }
  else if (tab === 'devices') {
    const r = await fetch('/api/devices'); const d = await r.json();
    document.getElementById('tbl-devices').innerHTML = d.map(v=>
      `<tr><td>${v.device_id}</td><td style="color:#666">${v.ip||'-'}</td>
       <td>${v.last_seen_s}s ago</td><td>${v.events}</td>
       <td class="${v.online?'ok':'offline'}">${v.online?'ONLINE':'OFFLINE'}</td></tr>`
    ).join('') || '<tr><td colspan="5" style="color:#444">No nodes</td></tr>';
  }
}

async function ackAlert(idx) {
  await fetch('/api/alerts/ack/'+idx, {method:'POST'});
  toast('Alert acknowledged');
  loadTab('alerts');
  loadSummary();
}

// Auto-refresh
loadSummary();
loadTab('networks');
setInterval(()=>{ loadSummary(); loadTab(currentTab); }, 5000);
</script>
</body></html>
)rawliteral";

// ─── Route helpers ────────────────────────────────────────────────────────
static void sendJson(AsyncWebServerRequest* req, const String& json, int code = 200) {
    req->send(code, "application/json", json);
}

static bool checkApiKey(AsyncWebServerRequest* req) {
    if (req->hasHeader("X-API-Key") &&
        req->header("X-API-Key") == DEVICE_API_KEY) return true;
    if (req->hasParam("key") &&
        req->getParam("key")->value() == DEVICE_API_KEY) return true;
    return false;
}

// ─── Body request handler ─────────────────────────────────────────────────
struct BodyRequest {
    AsyncWebServerRequest* req;
    String device_id;
    String body;
    bool   is_alerts;
};

static void handleBodyRequest(BodyRequest* br) {
    if (br->is_alerts)
        storeIngestAlertsJson(br->body, br->device_id);
    else
        storeIngestEventsJson(br->body, br->device_id);

    br->req->send(200, "application/json", "{\"ok\":true}");
    delete br;
}

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[NetSentryHub] %s  v%s\n", DEVICE_ID, DEVICE_VERSION);

    storeInit();
    sessionInit();
    displayInit();

    // SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("[SPIFFS] mount failed — continuing without file storage");
    }

    // WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);
    Serial.printf("[AP] SSID: %s  IP: %s\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str());

    // ── Routes ────────────────────────────────────────────────────────────

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/login");
    });

    server.on("/login", HTTP_GET, [](AsyncWebServerRequest* req) {
        String page = FPSTR(LOGIN_HTML);
        page.replace("%ERR%", "");
        req->send(200, "text/html", page);
    });

    server.on("/login", HTTP_POST, [](AsyncWebServerRequest* req) {
        String user = req->hasParam("user", true) ? req->getParam("user", true)->value() : "";
        String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";

        Role role = credentialCheck(user, pass);
        if (role == ROLE_NONE) {
            String page = FPSTR(LOGIN_HTML);
            page.replace("%ERR%", "<div class='err'>Invalid credentials</div>");
            req->send(401, "text/html", page);
            return;
        }

        String token = sessionCreate(role);
        AsyncWebServerResponse* res = req->beginResponse(302);
        res->addHeader("Location", "/dashboard");
        res->addHeader("Set-Cookie",
            "session=" + token + "; Path=/; HttpOnly; SameSite=Strict");
        req->send(res);
        Serial.printf("[AUTH] %s logged in as %s\n",
                      user.c_str(), role == ROLE_ADMIN ? "admin" : "operator");
    });

    server.on("/logout", HTTP_GET, [](AsyncWebServerRequest* req) {
        sessionDestroy(req);
        AsyncWebServerResponse* res = req->beginResponse(302);
        res->addHeader("Location", "/login");
        res->addHeader("Set-Cookie", "session=; Max-Age=0; Path=/");
        req->send(res);
    });

    server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        req->send(200, "text/html", FPSTR(DASHBOARD_HTML));
    });

    // ── Data API (browser) ────────────────────────────────────────────────
    server.on("/api/summary", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildSummaryJson(out); sendJson(req, out);
    });

    server.on("/api/networks", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildNetworksJson(out); sendJson(req, out);
    });

    server.on("/api/probes", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildProbesJson(out); sendJson(req, out);
    });

    server.on("/api/alerts", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildAlertsJson(out); sendJson(req, out);
    });

    server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildDevicesJson(out); sendJson(req, out);
    });

    server.on("/api/report.json", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildReportJson(out);
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", out);
        res->addHeader("Content-Disposition",
            "attachment; filename=\"netsentryhub_report.json\"");
        req->send(res);
    });

    server.on("/api/report.csv", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_OPERATOR)) return;
        String out; storeBuildReportCsv(out);
        AsyncWebServerResponse* res = req->beginResponse(200, "text/csv", out);
        res->addHeader("Content-Disposition",
            "attachment; filename=\"netsentryhub_report.csv\"");
        req->send(res);
    });

    // Alert acknowledge
    server.on("^\\/api\\/alerts\\/ack\\/([0-9]+)$", HTTP_POST,
        [](AsyncWebServerRequest* req) {
            if (!requireRole(req, ROLE_OPERATOR)) return;
            String idx_s = req->pathArg(0);
            storeAckAlert(idx_s.toInt());
            sendJson(req, "{\"ok\":true}");
        }
    );

    // Admin: clear all data
    server.on("/api/clear", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!requireRole(req, ROLE_ADMIN)) return;
        storeClearAll();
        sendJson(req, "{\"ok\":true,\"msg\":\"all data cleared\"}");
        Serial.println("[ADMIN] data cleared");
    });

    // ── Sensor node ingestion API ─────────────────────────────────────────
    // Nodes POST JSON arrays here. Auth via X-API-Key header.

    auto makeIngestHandler = [](bool is_alerts) {
        return [is_alerts](AsyncWebServerRequest* req, uint8_t* data,
                           size_t len, size_t index, size_t total) {
            if (!checkApiKey(req)) {
                req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
                return;
            }
            String device_id = req->hasParam("device_id")
                ? req->getParam("device_id")->value() : "unknown";

            String body = String((char*)data, len);
            BodyRequest* br = new BodyRequest{req, device_id, body, is_alerts};
            handleBodyRequest(br);
        };
    };

    server.on("/api/events", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        makeIngestHandler(false)
    );

    server.on("/api/alerts", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        makeIngestHandler(true)
    );

    // Node heartbeat / ping
    server.on("/api/ping", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!checkApiKey(req)) {
            req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        String device_id = req->hasParam("device_id")
            ? req->getParam("device_id")->value() : "unknown";
        storeIngestEventsJson("[]", device_id);
        sendJson(req, "{\"ok\":true,\"ts\":" + String(millis()) + "}");
    });

    // 404
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "not found");
    });

    server.beginSecure(SERVER_CERT_PEM, SERVER_KEY_PEM);
    Serial.println("[HTTPS] server started on port 443");
    Serial.printf("[NetSentryHub] ready — connect to '%s' then browse to https://%s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ─── Loop ─────────────────────────────────────────────────────────────────
static uint32_t last_display_ms  = 0;
static uint32_t last_cleanup_ms  = 0;

void loop() {
    if (millis() - last_display_ms >= DISPLAY_REFRESH_MS) {
        last_display_ms = millis();
        displayUpdate(
            storeNetCount(), storeProbeTotal(),
            storeAlertCount(), storeActiveAlertCount(),
            storeDeviceCount(),
            (uint8_t)WiFi.softAPgetStationNum(),
            true
        );
    }

    if (millis() - last_cleanup_ms >= 60000) {
        last_cleanup_ms = millis();
        sessionCleanup();
    }

    delay(10);
}
