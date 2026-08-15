#pragma once

// ─── Device identity ───────────────────────────────────────────────────────
#define DEVICE_ID        "sentry-hub-01"
#define DEVICE_VERSION   "1.0.0"

// ─── Access point ─────────────────────────────────────────────────────────
#define AP_SSID          "NetSentryHub"
// AP_PASSWORD, credentials, and DEVICE_API_KEY are in secrets.h (gitignored)
#define AP_CHANNEL       6
#define AP_MAX_CLIENTS   8

// ─── Web server ───────────────────────────────────────────────────────────
#define WEB_PORT         443

// ─── Secrets (credentials, API key) ──────────────────────────────────────
// Copy include/secrets.example.h → include/secrets.h and fill in real values.
#include "secrets.h"

// ─── Hardware ──────────────────────────────────────────────────────────────
#define OLED_CLK         18   // D0 — hardware SPI SCK
#define OLED_MOSI        23   // D1 — hardware SPI MOSI
#define OLED_RST         16   // RES
#define OLED_DC           4   // DC
#define OLED_CS           5   // CS
#define OLED_WIDTH       128
#define OLED_HEIGHT       64

// ─── Session ──────────────────────────────────────────────────────────────
#define SESSION_TIMEOUT_MS   (2 * 60 * 60 * 1000)   // 2 hours
#define MAX_SESSIONS         8

// ─── Data store capacity ──────────────────────────────────────────────────
#define MAX_NETWORKS     80
#define MAX_PROBES       200
#define MAX_DEAUTHS      50
#define MAX_ALERTS       80
#define MAX_DEVICES      20    // connected sensor nodes

// ─── Timing ────────────────────────────────────────────────────────────────
#define DISPLAY_REFRESH_MS   2000
#define NETWORK_EXPIRE_MS    (10 * 60 * 1000)
