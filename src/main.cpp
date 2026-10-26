#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "kline_channel.h"
#include "kwp_client.h"
#include "web.h"

// --- WiFi: fill in for your setup ---
static const char* WIFI_SSID = "CHANGE_ME";
static const char* WIFI_PASS = "CHANGE_ME";

KLineChannel kline(Serial2, /*rx*/16, /*tx*/17, /*nslp*/4, /*baud*/10400);
Protocol proto(&kline, Timing{});
KwpClient client(&proto);
WebServer server(80);
BridgeWeb web(server, kline, proto, client);

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
    web.begin();
}

void loop() {
    server.handleClient();
}
