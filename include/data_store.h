#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── Ingested record types ────────────────────────────────────────────────
struct NetRecord {
    char     ssid[33];
    char     bssid[18];
    char     vendor[18];
    char     enc[6];
    bool     wps;
    int8_t   rssi;
    uint8_t  channel;
    char     device_id[20];   // which sensor node sent this
    uint32_t ts;
};

struct ProbeRecord {
    char     src[18];
    char     vendor[18];
    char     seeking[33];
    int8_t   rssi;
    char     device_id[20];
    uint32_t ts;
};

struct DeauthRecord {
    char     src[18];
    char     bssid[18];
    uint16_t reason;
    int8_t   rssi;
    char     device_id[20];
    uint32_t ts;
};

struct AlertRecord {
    char     type[20];
    char     severity[6];
    char     detail[64];
    char     device_id[20];
    uint32_t ts;
    bool     acknowledged;
};

struct DeviceRecord {
    char     device_id[20];
    char     ip[16];
    uint32_t last_seen;
    uint32_t event_count;
};

// ─── Store API ────────────────────────────────────────────────────────────
void storeInit();

// Ingest — called from POST /api/events and /api/alerts
void storeIngestEventsJson(const String& json, const String& device_id);
void storeIngestAlertsJson(const String& json, const String& device_id);

// Query — called from GET /api/data
void storeBuildSummaryJson(String& out);
void storeBuildNetworksJson(String& out);
void storeBuildProbesJson(String& out, int limit = 50);
void storeBuildAlertsJson(String& out);
void storeBuildDevicesJson(String& out);
void storeBuildReportJson(String& out);
void storeBuildReportCsv(String& out);

void storeAckAlert(int idx);
void storeClearAll();

int storeNetCount();
int storeProbeTotal();
int storeAlertCount();
int storeActiveAlertCount();
int storeDeviceCount();
