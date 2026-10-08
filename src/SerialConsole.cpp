#include "SerialConsole.h"
#include <esp_system.h>

SerialConsole serialConsole;

SerialConsole::SerialConsole()
    : deauthDetector(nullptr), wifiIdsDetector(nullptr), appConfig(nullptr) {}

void SerialConsole::begin() {
    lineBuf.reserve(64);
}

void SerialConsole::attach(DeauthDetector* deauth, WifiIDSDetector* ids, AppConfig* config) {
    deauthDetector = deauth;
    wifiIdsDetector = ids;
    appConfig = config;
}

void SerialConsole::poll() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (lineBuf.length() > 0) {
                dispatch(lineBuf);
                lineBuf = "";
            }
        } else if (lineBuf.length() < 120) {
            lineBuf += c;
        }
    }
}

void SerialConsole::dispatch(const String& lineIn) {
    String line = lineIn;
    line.trim();
    line.toUpperCase();
    if (line == "HELP" || line == "?") {
        cmdHelp();
    } else if (line == "STATUS") {
        cmdStatus();
    } else if (line == "EVENTS") {
        cmdEvents();
    } else if (line == "IDS") {
        cmdIds();
    } else if (line == "CLEAR") {
        cmdClear();
    } else if (line == "VERSION") {
        cmdVersion();
    } else if (line.length() > 0) {
        Serial.println("CONSOLE: unknown command '" + line + "' (try HELP)");
    }
}

void SerialConsole::cmdHelp() {
    Serial.println("CONSOLE: commands:");
    Serial.println("  HELP     - this message");
    Serial.println("  VERSION  - firmware version + build identity");
    Serial.println("  STATUS   - monitoring state, heap, event counts");
    Serial.println("  EVENTS   - list buffered deauth events");
    Serial.println("  IDS      - list buffered WifiIDS events (evil-twin/karma/etc)");
    Serial.println("  CLEAR    - clear all buffered events (both detectors)");
}

void SerialConsole::cmdVersion() {
    Serial.println("CONSOLE: === M5 Cardputer Deauth Detector ===");
    Serial.print("CONSOLE: Firmware v");
    Serial.println(FIRMWARE_VERSION);
}

void SerialConsole::cmdStatus() {
    Serial.println("CONSOLE: --- status ---");
    Serial.print("CONSOLE: uptime_ms=");
    Serial.println(millis());
    Serial.print("CONSOLE: free_heap_bytes=");
    Serial.println(ESP.getFreeHeap());
    if (deauthDetector) {
        Serial.print("CONSOLE: deauth_monitoring=");
        Serial.println(deauthDetector->isMonitoring() ? "true" : "false");
        Serial.print("CONSOLE: deauth_events_buffered=");
        Serial.println(deauthDetector->hasEvents() ? "true" : "false");
    }
    if (wifiIdsDetector) {
        Serial.print("CONSOLE: wifi_ids_events_buffered=");
        Serial.println(wifiIdsDetector->hasEvents() ? "true" : "false");
    }
    if (appConfig) {
        Serial.print("CONSOLE: wifi_ids_enabled=");
        Serial.println(appConfig->wifi_ids.enabled ? "true" : "false");
        Serial.print("CONSOLE: protected_ssids_count=");
        Serial.println((int)appConfig->detection.protected_ssids.size());
        Serial.print("CONSOLE: api_endpoint=");
        Serial.println(appConfig->api.endpoint_url);
    }
}

void SerialConsole::cmdEvents() {
    if (!deauthDetector) {
        Serial.println("CONSOLE: deauth detector not attached");
        return;
    }
    std::vector<DeauthEvent> evts = deauthDetector->getEvents();
    Serial.print("CONSOLE: deauth_event_count=");
    Serial.println((int)evts.size());
    for (const DeauthEvent& e : evts) {
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "CONSOLE: deauth ssid=%s bssid=%s attacker=%s ch=%d rssi=%d count=%d",
                 e.target_ssid.c_str(), e.target_bssid.c_str(), e.attacker_mac.c_str(),
                 e.channel, e.rssi, e.packet_count);
        Serial.println(buf);
    }
}

void SerialConsole::cmdIds() {
    if (!wifiIdsDetector) {
        Serial.println("CONSOLE: WifiIDS detector not attached");
        return;
    }
    std::vector<WifiIDSEvent> evts = wifiIdsDetector->getEvents();
    Serial.print("CONSOLE: wifi_ids_event_count=");
    Serial.println((int)evts.size());
    for (const WifiIDSEvent& e : evts) {
        char buf[200];
        snprintf(buf, sizeof(buf),
                 "CONSOLE: ids detector=%s sev=%s ssid=%s bssid=%s ch=%d rssi=%d summary=\"%s\"",
                 e.detector.c_str(), e.severity.c_str(), e.ssid.c_str(), e.bssid.c_str(),
                 e.channel, e.rssi, e.summary.c_str());
        Serial.println(buf);
    }
}

void SerialConsole::cmdClear() {
    if (deauthDetector) deauthDetector->clearEvents();
    if (wifiIdsDetector) wifiIdsDetector->clearEvents();
    Serial.println("CONSOLE: cleared");
}
