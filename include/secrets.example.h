#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  NetSentryHub — secrets template
//  Copy this file to secrets.h and fill in real values before building.
//  secrets.h is gitignored and must NOT be committed to source control.
// ─────────────────────────────────────────────────────────────────────────────

// ─── WiFi AP password ─────────────────────────────────────────────────────────
#define AP_PASSWORD       "change_me_min8chars"

// ─── Web admin credentials ────────────────────────────────────────────────────
#define ADMIN_USER        "admin"
#define ADMIN_PASS        "change_me_admin_password"

// ─── Operator-role credentials ────────────────────────────────────────────────
#define OPERATOR_USER     "operator"
#define OPERATOR_PASS     "change_me_operator_password"

// ─── Device API key (sensor nodes use this to POST events) ───────────────────
// Must match the API key configured on every reporting sensor node.
#define DEVICE_API_KEY    "change_me_device_api_key"
