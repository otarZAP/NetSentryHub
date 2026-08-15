#include "display_mgr.h"
#include "config.h"
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <WiFi.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &SPI, OLED_DC, OLED_RST, OLED_CS);

void displayInit() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println("[OLED] init failed");
        return;
    }
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(4, 8);
    oled.print("SentryHub");
    oled.setTextSize(1);
    oled.setCursor(20, 30);
    oled.print(DEVICE_VERSION);
    oled.setCursor(5, 44);
    oled.print("WiFi IDS Aggregator");
    oled.display();
    delay(2500);
}

void displayUpdate(int nets, int probes, int alerts, int active_alerts,
                   int devices, uint8_t clients, bool ap_ok) {
    oled.clearDisplay();

    // Header bar
    oled.fillRect(0, 0, OLED_WIDTH, 9, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(1, 1);
    oled.setTextSize(1);
    oled.print("SentryHub");
    char ip[16];
    WiFi.softAPIP().toString().toCharArray(ip, 16);
    oled.setCursor(58, 1);
    oled.print(ip);
    oled.setTextColor(SSD1306_WHITE);

    oled.setTextSize(1);
    char line[22];

    // AP status
    oled.setCursor(0, 12);
    snprintf(line, sizeof(line), "AP: %-3s  Clients: %d",
             ap_ok ? "UP" : "ERR", clients);
    oled.print(line);

    // Sensor nodes
    oled.setCursor(0, 22);
    snprintf(line, sizeof(line), "Nodes   : %d", devices);
    oled.print(line);

    // Recon counts
    oled.setCursor(0, 32);
    snprintf(line, sizeof(line), "Networks: %-4d Probes:%d", nets, probes);
    oled.print(line);

    // Alerts — invert if active
    if (active_alerts > 0) {
        oled.fillRect(0, 42, OLED_WIDTH, 10, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
    }
    oled.setCursor(0, 43);
    snprintf(line, sizeof(line), "Alerts: %d  Active: %d", alerts, active_alerts);
    oled.print(line);
    oled.setTextColor(SSD1306_WHITE);

    // Footer
    oled.fillRect(0, 55, OLED_WIDTH, 9, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(1, 56);
    oled.print("browse: ");
    oled.print(ip);
    oled.setTextColor(SSD1306_WHITE);

    oled.display();
}
