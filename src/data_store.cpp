#include "data_store.h"
#include <string.h>

static NetRecord    networks[MAX_NETWORKS];
static int          net_count    = 0;

static ProbeRecord  probes[MAX_PROBES];
static int          probe_head   = 0;
static int          probe_total  = 0;

static DeauthRecord deauths[MAX_DEAUTHS];
static int          deauth_count = 0;

static AlertRecord  alerts[MAX_ALERTS];
static int          alert_count  = 0;

static DeviceRecord devices[MAX_DEVICES];
static int          device_count = 0;

static SemaphoreHandle_t store_mutex = nullptr;

// ─── Helpers ──────────────────────────────────────────────────────────────
static void safeStrCpy(char* dst, const char* src, size_t max) {
    strncpy(dst, src, max - 1);
    dst[max - 1] = '\0';
}

static void updateDevice(const char* device_id, const char* ip) {
    for (int i = 0; i < device_count; i++) {
        if (strcmp(devices[i].device_id, device_id) == 0) {
            devices[i].last_seen = millis();
            devices[i].event_count++;
            if (ip) safeStrCpy(devices[i].ip, ip, 16);
            return;
        }
    }
    if (device_count < MAX_DEVICES) {
        safeStrCpy(devices[device_count].device_id, device_id, 20);
        if (ip) safeStrCpy(devices[device_count].ip, ip, 16);
        else devices[device_count].ip[0] = '\0';
        devices[device_count].last_seen  = millis();
        devices[device_count].event_count = 1;
        device_count++;
    }
}

static void addNet(JsonObject obj, const char* device_id) {
    const char* bssid = obj["bssid"] | "";
    // Update if already exists
    for (int i = 0; i < net_count; i++) {
        if (strcmp(networks[i].bssid, bssid) == 0) {
            networks[i].rssi    = obj["rssi"] | networks[i].rssi;
            networks[i].ts      = millis();
            return;
        }
    }
    if (net_count >= MAX_NETWORKS) return;
    NetRecord& n = networks[net_count++];
    safeStrCpy(n.ssid,      obj["ssid"]   | "",      33);
    safeStrCpy(n.bssid,     bssid,                   18);
    safeStrCpy(n.vendor,    obj["vendor"] | "?",     18);
    safeStrCpy(n.enc,       obj["enc"]    | "?",      6);
    safeStrCpy(n.device_id, device_id,               20);
    n.wps     = obj["wps"]     | false;
    n.rssi    = obj["rssi"]    | 0;
    n.channel = obj["channel"] | 0;
    n.ts      = millis();
}

static void addProbe(JsonObject obj, const char* device_id) {
    ProbeRecord& p = probes[probe_head];
    safeStrCpy(p.src,       obj["src"]     | "",  18);
    safeStrCpy(p.vendor,    obj["vendor"]  | "?", 18);
    safeStrCpy(p.seeking,   obj["seeking"] | "",  33);
    safeStrCpy(p.device_id, device_id,            20);
    p.rssi = obj["rssi"] | 0;
    p.ts   = millis();
    probe_head = (probe_head + 1) % MAX_PROBES;
    probe_total++;
}

static void addDeauth(JsonObject obj, const char* device_id) {
    if (deauth_count >= MAX_DEAUTHS) {
        memmove(deauths, deauths + 1, sizeof(DeauthRecord) * (MAX_DEAUTHS - 1));
        deauth_count = MAX_DEAUTHS - 1;
    }
    DeauthRecord& d = deauths[deauth_count++];
    safeStrCpy(d.src,       obj["src"]   | "",   18);
    safeStrCpy(d.bssid,     obj["bssid"] | "",   18);
    safeStrCpy(d.device_id, device_id,           20);
    d.reason = obj["reason"] | 0;
    d.rssi   = obj["rssi"]   | 0;
    d.ts     = millis();
}

// ─── Public API ──────────────────────────────────────────────────────────
void storeInit() {
    store_mutex = xSemaphoreCreateMutex();
    memset(networks, 0, sizeof(networks));
    memset(probes,   0, sizeof(probes));
    memset(deauths,  0, sizeof(deauths));
    memset(alerts,   0, sizeof(alerts));
    memset(devices,  0, sizeof(devices));
}

void storeIngestEventsJson(const String& json, const String& device_id) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    updateDevice(device_id.c_str(), nullptr);

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        const char* type = obj["type"] | "";
        if      (strcmp(type, "network") == 0) addNet(obj, device_id.c_str());
        else if (strcmp(type, "probe")   == 0) addProbe(obj, device_id.c_str());
        else if (strcmp(type, "deauth")  == 0) addDeauth(obj, device_id.c_str());
    }
    xSemaphoreGive(store_mutex);
}

void storeIngestAlertsJson(const String& json, const String& device_id) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    xSemaphoreTake(store_mutex, portMAX_DELAY);
    updateDevice(device_id.c_str(), nullptr);

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        if (alert_count >= MAX_ALERTS) break;
        AlertRecord& a = alerts[alert_count++];
        safeStrCpy(a.type,      obj["type"]   | "UNKNOWN", 20);
        safeStrCpy(a.severity,  obj["sev"]    | "INFO",     6);
        safeStrCpy(a.detail,    obj["detail"] | "",         64);
        safeStrCpy(a.device_id, device_id.c_str(),          20);
        a.ts           = millis();
        a.acknowledged = false;
    }
    xSemaphoreGive(store_mutex);
}

void storeBuildSummaryJson(String& out) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    JsonDocument doc;
    doc["networks"]      = net_count;
    doc["probes"]        = probe_total;
    doc["deauths"]       = deauth_count;
    doc["alerts"]        = alert_count;
    doc["active_alerts"] = storeActiveAlertCount();
    doc["devices"]       = device_count;
    doc["uptime_ms"]     = millis();

    int open_n = 0, wep_n = 0, wps_n = 0;
    for (int i = 0; i < net_count; i++) {
        if (strcmp(networks[i].enc, "OPEN") == 0) open_n++;
        if (strcmp(networks[i].enc, "WEP")  == 0) wep_n++;
        if (networks[i].wps)                       wps_n++;
    }
    doc["open_networks"] = open_n;
    doc["wep_networks"]  = wep_n;
    doc["wps_aps"]       = wps_n;

    serializeJson(doc, out);
    xSemaphoreGive(store_mutex);
}

void storeBuildNetworksJson(String& out) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < net_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"]      = networks[i].ssid;
        o["bssid"]     = networks[i].bssid;
        o["vendor"]    = networks[i].vendor;
        o["enc"]       = networks[i].enc;
        o["wps"]       = networks[i].wps;
        o["rssi"]      = networks[i].rssi;
        o["channel"]   = networks[i].channel;
        o["device_id"] = networks[i].device_id;
        o["ts"]        = networks[i].ts;
    }
    serializeJson(doc, out);
    xSemaphoreGive(store_mutex);
}

void storeBuildProbesJson(String& out, int limit) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    int cap   = (probe_total < MAX_PROBES) ? probe_total : MAX_PROBES;
    int count = (cap < limit) ? cap : limit;
    for (int i = 0; i < count; i++) {
        int idx = ((probe_head - 1 - i) + MAX_PROBES) % MAX_PROBES;
        JsonObject o = arr.add<JsonObject>();
        o["src"]       = probes[idx].src;
        o["vendor"]    = probes[idx].vendor;
        o["seeking"]   = probes[idx].seeking;
        o["rssi"]      = probes[idx].rssi;
        o["device_id"] = probes[idx].device_id;
        o["ts"]        = probes[idx].ts;
    }
    serializeJson(doc, out);
    xSemaphoreGive(store_mutex);
}

void storeBuildAlertsJson(String& out) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < alert_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["idx"]       = i;
        o["type"]      = alerts[i].type;
        o["severity"]  = alerts[i].severity;
        o["detail"]    = alerts[i].detail;
        o["device_id"] = alerts[i].device_id;
        o["ts"]        = alerts[i].ts;
        o["acked"]     = alerts[i].acknowledged;
    }
    serializeJson(doc, out);
    xSemaphoreGive(store_mutex);
}

void storeBuildDevicesJson(String& out) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    uint32_t now = millis();
    for (int i = 0; i < device_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["device_id"]   = devices[i].device_id;
        o["ip"]          = devices[i].ip;
        o["last_seen_s"] = (now - devices[i].last_seen) / 1000;
        o["events"]      = devices[i].event_count;
        o["online"]      = (now - devices[i].last_seen) < 120000;
    }
    serializeJson(doc, out);
    xSemaphoreGive(store_mutex);
}

void storeBuildReportJson(String& out) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    out = "{";
    String tmp;
    storeBuildSummaryJson(tmp);   out += "\"summary\":" + tmp + ",";
    xSemaphoreGive(store_mutex);
    storeBuildNetworksJson(tmp);  out += "\"networks\":" + tmp + ",";
    storeBuildAlertsJson(tmp);    out += "\"alerts\":"   + tmp + ",";
    storeBuildProbesJson(tmp, 100); out += "\"probes\":"   + tmp;
    out += "}";
}

void storeBuildReportCsv(String& out) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    out = "type,ssid,bssid,vendor,enc,wps,rssi,channel,device_id\n";
    for (int i = 0; i < net_count; i++) {
        out += "network,";
        out += String(networks[i].ssid) + ",";
        out += String(networks[i].bssid) + ",";
        out += String(networks[i].vendor) + ",";
        out += String(networks[i].enc) + ",";
        out += String(networks[i].wps ? "yes" : "no") + ",";
        out += String(networks[i].rssi) + ",";
        out += String(networks[i].channel) + ",";
        out += String(networks[i].device_id) + "\n";
    }
    xSemaphoreGive(store_mutex);
}

void storeAckAlert(int idx) {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    if (idx >= 0 && idx < alert_count) alerts[idx].acknowledged = true;
    xSemaphoreGive(store_mutex);
}

void storeClearAll() {
    xSemaphoreTake(store_mutex, portMAX_DELAY);
    net_count = probe_head = probe_total = deauth_count = alert_count = device_count = 0;
    xSemaphoreGive(store_mutex);
}

int storeNetCount()         { return net_count; }
int storeProbeTotal()       { return probe_total; }
int storeAlertCount()       { return alert_count; }
int storeDeviceCount()      { return device_count; }
int storeActiveAlertCount() {
    int n = 0;
    for (int i = 0; i < alert_count; i++) {
        if (!alerts[i].acknowledged) n++;
    }
    return n;
}
