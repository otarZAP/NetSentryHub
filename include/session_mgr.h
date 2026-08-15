#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

typedef enum : uint8_t {
    ROLE_NONE     = 0,
    ROLE_OPERATOR = 1,
    ROLE_ADMIN    = 2,
} Role;

struct Session {
    char     token[33];
    Role     role;
    uint32_t expires;
    bool     active;
};

void     sessionInit();
String   sessionCreate(Role role);
Role     sessionCheck(AsyncWebServerRequest* req);
void     sessionDestroy(AsyncWebServerRequest* req);
void     sessionCleanup();
bool     requireRole(AsyncWebServerRequest* req, Role min_role);
Role     credentialCheck(const String& user, const String& pass);
