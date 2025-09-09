#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

static const char* MDNS_HOST = "aria";   // will be reachable at http://aria.local
AsyncWebServer server(80);

String wifiSSID, wifiPASS;

bool loadWiFiCredentials() {
    File f = SPIFFS.open("/wifi.json", "r");
    if (!f) {
        Serial.println("wifi.json not found in SPIFFS");
        return false;
    }
    JsonDocument doc;                               // ArduinoJson v7
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.print("wifi.json parse error: ");
        Serial.println(err.f_str());
        return false;
    }
    if (!doc.containsKey("ssid") || !doc.containsKey("password")) {
        Serial.println("wifi.json missing ssid/password");
        return false;
    }
    wifiSSID = (const char*)doc["ssid"];
    wifiPASS = (const char*)doc["password"];
    return true;
}

bool connectWiFi(uint32_t timeout_ms = 20000) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
    Serial.print("Connecting to ");
    Serial.print(wifiSSID);
    Serial.print(" ");
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi OK, IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }
    Serial.println("WiFi connect timeout");
    return false;
}

void setupWeb() {
    // serve / -> /index.html from SPIFFS
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument j;
        j["ip"]   = WiFi.isConnected() ? WiFi.localIP().toString() : String("0.0.0.0");
        j["host"] = MDNS_HOST;
        String out;
        serializeJson(j, out);
        req->send(200, "application/json", out);
    });

    server.onNotFound([](AsyncWebServerRequest* req){
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    Serial.println("HTTP server started");
}

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.println("\nBoot");

    if (!SPIFFS.begin(true)) {   // format if needed
        Serial.println("SPIFFS mount failed");
        return;
    }

    if (!loadWiFiCredentials()) {
        Serial.println("Create /wifi.json in SPIFFS and upload with `pio run -t uploadfs`.");
        return;
    }

    if (!connectWiFi()) {
        Serial.println("Continuing without WiFi.");
        // Optionally: start AP here if you want a fallback.
    }

    if (WiFi.isConnected()) {
        if (MDNS.begin(MDNS_HOST)) {
            Serial.print("mDNS started: http://");
            Serial.print(MDNS_HOST);
            Serial.println(".local/");
        } else {
            Serial.println("mDNS start failed");
        }
    }

    setupWeb();
}

void loop() {
    // no work needed; Async server + mDNS run in background
}