#include "session_mgr.h"
#include <esp_random.h>
#include <string.h>

static Session sessions[MAX_SESSIONS];

void sessionInit() {
    memset(sessions, 0, sizeof(sessions));
}

static void generateToken(char* buf) {
    // 32 hex chars from hardware RNG
    uint32_t a = esp_random(), b = esp_random(),
             c = esp_random(), d = esp_random();
    snprintf(buf, 33, "%08X%08X%08X%08X", a, b, c, d);
}

String sessionCreate(Role role) {
    sessionCleanup();

    // Find free slot
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            generateToken(sessions[i].token);
            sessions[i].role    = role;
            sessions[i].expires = millis() + SESSION_TIMEOUT_MS;
            sessions[i].active  = true;
            return String(sessions[i].token);
        }
    }
    // All full — evict oldest
    uint32_t oldest_ts = UINT32_MAX;
    int oldest_idx = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].expires < oldest_ts) {
            oldest_ts  = sessions[i].expires;
            oldest_idx = i;
        }
    }
    generateToken(sessions[oldest_idx].token);
    sessions[oldest_idx].role    = role;
    sessions[oldest_idx].expires = millis() + SESSION_TIMEOUT_MS;
    sessions[oldest_idx].active  = true;
    return String(sessions[oldest_idx].token);
}

Role sessionCheck(AsyncWebServerRequest* req) {
    if (!req->hasHeader("Cookie")) return ROLE_NONE;
    String cookie = req->header("Cookie");
    int idx = cookie.indexOf("session=");
    if (idx < 0) return ROLE_NONE;

    String token = cookie.substring(idx + 8);
    int end = token.indexOf(';');
    if (end >= 0) token = token.substring(0, end);
    token.trim();

    uint32_t now = millis();
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            strcmp(sessions[i].token, token.c_str()) == 0) {
            if (now > sessions[i].expires) {
                sessions[i].active = false;
                return ROLE_NONE;
            }
            // Refresh on activity
            sessions[i].expires = now + SESSION_TIMEOUT_MS;
            return sessions[i].role;
        }
    }
    return ROLE_NONE;
}

void sessionDestroy(AsyncWebServerRequest* req) {
    if (!req->hasHeader("Cookie")) return;
    String cookie = req->header("Cookie");
    int idx = cookie.indexOf("session=");
    if (idx < 0) return;
    String token = cookie.substring(idx + 8);
    int end = token.indexOf(';');
    if (end >= 0) token = token.substring(0, end);
    token.trim();

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            strcmp(sessions[i].token, token.c_str()) == 0) {
            sessions[i].active = false;
            return;
        }
    }
}

void sessionCleanup() {
    uint32_t now = millis();
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && now > sessions[i].expires) {
            sessions[i].active = false;
        }
    }
}

bool requireRole(AsyncWebServerRequest* req, Role min_role) {
    Role r = sessionCheck(req);
    if (r < min_role) {
        req->redirect("/login");
        return false;
    }
    return true;
}

Role credentialCheck(const String& user, const String& pass) {
    if (user == ADMIN_USER    && pass == ADMIN_PASS)    return ROLE_ADMIN;
    if (user == OPERATOR_USER && pass == OPERATOR_PASS) return ROLE_OPERATOR;
    return ROLE_NONE;
}
