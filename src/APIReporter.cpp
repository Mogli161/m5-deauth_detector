#include "APIReporter.h"
#include "Logger.h"

APIReporter::APIReporter(APIConfig& config) : apiConfig(config) {}

bool APIReporter::sendBatch(const std::vector<DeauthEvent>& events) {
    if (events.empty()) {
        logger.debugPrintln("No events to report");
        return true;
    }
    
    if (apiConfig.endpoint_url.isEmpty()) {
        logger.debugPrintln("API endpoint not configured");
        return false;
    }
    
    HTTPClient http;
    http.begin(apiConfig.endpoint_url);
    http.addHeader("Content-Type", "application/json");
    
    // Add custom header if configured
    if (!apiConfig.custom_header_name.isEmpty() && !apiConfig.custom_header_value.isEmpty()) {
        http.addHeader(apiConfig.custom_header_name, apiConfig.custom_header_value);
    }
    
    String payload = buildPayload(events);
    
    logger.debugPrint("Sending ");
    logger.debugPrint(String(events.size()));
    logger.debugPrintln(" events to API...");
    logger.debugPrintln(payload);
    
    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode > 0) {
        logger.debugPrint("API response code: ");
        logger.debugPrintln(String(httpResponseCode));
        
        String response = http.getString();
        logger.debugPrintln("Response: " + response);
        
        http.end();
        return (httpResponseCode >= 200 && httpResponseCode < 300);
    } else {
        logger.debugPrint("Error sending to API: ");
        logger.debugPrintln(http.errorToString(httpResponseCode));
        http.end();
        return false;
    }
}

String APIReporter::buildPayload(const std::vector<DeauthEvent>& events) {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    
    for (const DeauthEvent& event : events) {
        JsonObject obj = array.createNestedObject();
        
        // Format timestamp as ISO 8601
        char timestamp[32];
        struct tm timeinfo;
        localtime_r(&event.timestamp, &timeinfo);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        
        obj["timestamp"] = timestamp;
        obj["event_type"] = "deauth";
        obj["target_ssid"] = event.target_ssid;
        obj["target_bssid"] = event.target_bssid;
        obj["attacker_mac"] = event.attacker_mac;
        obj["channel"] = event.channel;
        obj["rssi"] = event.rssi;
        obj["packet_count"] = event.packet_count;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool APIReporter::sendIdsBatch(const std::vector<WifiIDSEvent>& events) {
    if (events.empty()) {
        logger.debugPrintln("No WifiIDS events to report");
        return true;
    }

    if (apiConfig.endpoint_url.isEmpty()) {
        logger.debugPrintln("API endpoint not configured");
        return false;
    }

    HTTPClient http;
    http.begin(apiConfig.endpoint_url);
    http.addHeader("Content-Type", "application/json");

    if (!apiConfig.custom_header_name.isEmpty() && !apiConfig.custom_header_value.isEmpty()) {
        http.addHeader(apiConfig.custom_header_name, apiConfig.custom_header_value);
    }

    String payload = buildIdsPayload(events);

    logger.debugPrint("Sending ");
    logger.debugPrint(String(events.size()));
    logger.debugPrintln(" WifiIDS events to API...");
    logger.debugPrintln(payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        logger.debugPrint("API response code: ");
        logger.debugPrintln(String(httpResponseCode));

        String response = http.getString();
        logger.debugPrintln("Response: " + response);

        http.end();
        return (httpResponseCode >= 200 && httpResponseCode < 300);
    } else {
        logger.debugPrint("Error sending to API: ");
        logger.debugPrintln(http.errorToString(httpResponseCode));
        http.end();
        return false;
    }
}

String APIReporter::buildIdsPayload(const std::vector<WifiIDSEvent>& events) {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();

    for (const WifiIDSEvent& event : events) {
        JsonObject obj = array.createNestedObject();

        char timestamp[32];
        struct tm timeinfo;
        localtime_r(&event.timestamp, &timeinfo);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

        obj["timestamp"] = timestamp;
        obj["event_type"] = event.detector; // beacon_flood | evil_twin | karma | pnl_leak
        obj["severity"] = event.severity;
        obj["target_ssid"] = event.ssid;
        obj["target_bssid"] = event.bssid;
        obj["channel"] = event.channel;
        obj["rssi"] = event.rssi;
        obj["summary"] = event.summary;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
