#include "ConfigManager.h"
#include "Logger.h"

ConfigManager::ConfigManager() : configValid(false) {
    setDefaults();
}

void ConfigManager::setDefaults() {
    config.wifi.sta_ssid = "";
    config.wifi.sta_password = "";
    config.wifi.admin_user = "admin";
    config.wifi.admin_pass = "1234";
    
    config.ntp.server = "pool.ntp.org";
    config.ntp.timezone_offset = 0;
    config.ntp.daylight_savings = false;
    
    config.detection.reporting_interval_seconds = 10;
    config.detection.packet_threshold = DEFAULT_PACKET_THRESHOLD;
    config.detection.detect_all_deauth = false;
    config.detection.channel_scan_time_ms = DEFAULT_CHANNEL_SCAN_TIME_MS;
    config.detection.channel_hop_interval_ms = DEFAULT_CHANNEL_HOP_INTERVAL_MS;
    
    config.api.endpoint_url = "";
    config.api.custom_header_name = "X-API-KEY";
    config.api.custom_header_value = "";
    
    config.hardware.buzzer_freq = 2000;
    config.hardware.buzzer_duration_ms = 2000;
    config.hardware.screen_brightness = 128;
    config.hardware.fancy_intro = true;
    
    config.debug.enabled = false;
}

bool ConfigManager::loadConfig(const char* filename) {
    if (!SD.exists(filename)) {
        logger.debugPrintln("Config file not found");
        return false;
    }
    
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        logger.debugPrintln("Failed to open config file");
        return false;
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        logger.debugPrint("Failed to parse config: ");
        logger.debugPrintln(error.c_str());
        return false;
    }
    
    // Parse WiFi config
    if (doc.containsKey("wifi")) {
        JsonObject wifi = doc["wifi"];
        config.wifi.sta_ssid = wifi["sta_ssid"] | "";
        config.wifi.sta_password = wifi["sta_password"] | "";
        config.wifi.admin_user = wifi["admin_user"] | "admin";
        config.wifi.admin_pass = wifi["admin_pass"] | "1234";
    }
    
    // Parse NTP config
    if (doc.containsKey("ntp")) {
        JsonObject ntp = doc["ntp"];
        config.ntp.server = ntp["server"] | "pool.ntp.org";
        config.ntp.timezone_offset = ntp["timezone_offset"] | 0;
        config.ntp.daylight_savings = ntp["daylight_savings"] | false;
    }
    
    // Parse Detection config
    if (doc.containsKey("detection")) {
        JsonObject detection = doc["detection"];
        
        config.detection.protected_ssids.clear();
        if (detection.containsKey("protected_ssids")) {
            JsonArray ssids = detection["protected_ssids"];
            for (JsonVariant v : ssids) {
                config.detection.protected_ssids.push_back(v.as<String>());
            }
        }
        
        config.detection.reporting_interval_seconds = detection["reporting_interval_seconds"] | 10;
        config.detection.packet_threshold = detection["packet_threshold"] | DEFAULT_PACKET_THRESHOLD;
        config.detection.detect_all_deauth = detection["detect_all_deauth"] | false;
        config.detection.channel_scan_time_ms = detection["channel_scan_time_ms"] | DEFAULT_CHANNEL_SCAN_TIME_MS;
        config.detection.channel_hop_interval_ms = detection["channel_hop_interval_ms"] | DEFAULT_CHANNEL_HOP_INTERVAL_MS;
    }
    
    // Parse WifiIDS config (beacon-flood/evil-twin/karma/pnl-leak)
    if (doc.containsKey("wifi_ids")) {
        JsonObject ids = doc["wifi_ids"];
        config.wifi_ids.enabled = ids["enabled"] | false;
        config.wifi_ids.beacon_window_sec = ids["beacon_window_sec"] | 60;
        config.wifi_ids.beacon_new_bssid_burst = ids["beacon_new_bssid_burst"] | 6;
        config.wifi_ids.beacon_la_ratio = ids["beacon_la_ratio"] | 0.6f;
        config.wifi_ids.karma_window_sec = ids["karma_window_sec"] | 120;
        config.wifi_ids.karma_distinct_threshold = ids["karma_distinct_threshold"] | 2;
        config.wifi_ids.pnl_window_sec = ids["pnl_window_sec"] | 120;
        config.wifi_ids.pnl_distinct_threshold = ids["pnl_distinct_threshold"] | 3;
        config.wifi_ids.refractory_sec = ids["refractory_sec"] | 300;

        config.wifi_ids.allowlist.clear();
        if (ids.containsKey("allowlist")) {
            // allowlist is a JSON object: { "SSID": ["BSSID1", "BSSID2"], ... }
            JsonObject allow = ids["allowlist"];
            for (JsonPair kv : allow) {
                AllowedNetwork net;
                net.ssid = String(kv.key().c_str());
                JsonArray bssids = kv.value().as<JsonArray>();
                for (JsonVariant b : bssids) {
                    net.bssids.push_back(b.as<String>());
                }
                config.wifi_ids.allowlist.push_back(net);
            }
        }
    }
    
    // Parse API config
    if (doc.containsKey("api")) {
        JsonObject api = doc["api"];
        config.api.endpoint_url = api["endpoint_url"] | "";
        config.api.custom_header_name = api["custom_header_name"] | "X-API-KEY";
        config.api.custom_header_value = api["custom_header_value"] | "";
    }
    
    // Parse Hardware config
    if (doc.containsKey("hardware")) {
        JsonObject hardware = doc["hardware"];
        config.hardware.buzzer_freq = hardware["buzzer_freq"] | 2000;
        config.hardware.buzzer_duration_ms = hardware["buzzer_duration_ms"] | 2000;
        config.hardware.screen_brightness = hardware["screen_brightness"] | 128;
        config.hardware.fancy_intro = hardware["fancy_intro"] | true;
    }
    
    // Parse Debug config
    if (doc.containsKey("debug")) {
        JsonObject debug = doc["debug"];
        config.debug.enabled = debug["enabled"] | false;
    }
    
    configValid = !config.wifi.sta_ssid.isEmpty() && 
                  !config.detection.protected_ssids.empty();
    
    logger.debugPrintln("Config loaded successfully");
    return configValid;
}

bool ConfigManager::saveConfig(const char* filename) {
    StaticJsonDocument<2048> doc;
    
    // WiFi config
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["sta_ssid"] = config.wifi.sta_ssid;
    wifi["sta_password"] = config.wifi.sta_password;
    wifi["admin_user"] = config.wifi.admin_user;
    wifi["admin_pass"] = config.wifi.admin_pass;
    
    // NTP config
    JsonObject ntp = doc.createNestedObject("ntp");
    ntp["server"] = config.ntp.server;
    ntp["timezone_offset"] = config.ntp.timezone_offset;
    ntp["daylight_savings"] = config.ntp.daylight_savings;
    
    // Detection config
    JsonObject detection = doc.createNestedObject("detection");
    JsonArray ssids = detection.createNestedArray("protected_ssids");
    for (const String& ssid : config.detection.protected_ssids) {
        ssids.add(ssid);
    }
    detection["reporting_interval_seconds"] = config.detection.reporting_interval_seconds;
    detection["packet_threshold"] = config.detection.packet_threshold;
    detection["detect_all_deauth"] = config.detection.detect_all_deauth;
    detection["channel_scan_time_ms"] = config.detection.channel_scan_time_ms;
    detection["channel_hop_interval_ms"] = config.detection.channel_hop_interval_ms;
    
    // WifiIDS config
    JsonObject ids = doc.createNestedObject("wifi_ids");
    ids["enabled"] = config.wifi_ids.enabled;
    ids["beacon_window_sec"] = config.wifi_ids.beacon_window_sec;
    ids["beacon_new_bssid_burst"] = config.wifi_ids.beacon_new_bssid_burst;
    ids["beacon_la_ratio"] = config.wifi_ids.beacon_la_ratio;
    ids["karma_window_sec"] = config.wifi_ids.karma_window_sec;
    ids["karma_distinct_threshold"] = config.wifi_ids.karma_distinct_threshold;
    ids["pnl_window_sec"] = config.wifi_ids.pnl_window_sec;
    ids["pnl_distinct_threshold"] = config.wifi_ids.pnl_distinct_threshold;
    ids["refractory_sec"] = config.wifi_ids.refractory_sec;
    JsonObject allow = ids.createNestedObject("allowlist");
    for (const AllowedNetwork& net : config.wifi_ids.allowlist) {
        JsonArray bssids = allow.createNestedArray(net.ssid);
        for (const String& b : net.bssids) {
            bssids.add(b);
        }
    }
    
    // API config
    JsonObject api = doc.createNestedObject("api");
    api["endpoint_url"] = config.api.endpoint_url;
    api["custom_header_name"] = config.api.custom_header_name;
    api["custom_header_value"] = config.api.custom_header_value;
    
    // Hardware config
    JsonObject hardware = doc.createNestedObject("hardware");
    hardware["buzzer_freq"] = config.hardware.buzzer_freq;
    hardware["buzzer_duration_ms"] = config.hardware.buzzer_duration_ms;
    hardware["screen_brightness"] = config.hardware.screen_brightness;
    hardware["fancy_intro"] = config.hardware.fancy_intro;
    
    // Debug config
    JsonObject debug = doc.createNestedObject("debug");
    debug["enabled"] = config.debug.enabled;
    
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        logger.debugPrintln("Failed to create config file");
        return false;
    }
    
    if (serializeJsonPretty(doc, file) == 0) {
        logger.debugPrintln("Failed to write config");
        file.close();
        return false;
    }
    
    file.close();
    logger.debugPrintln("Config saved successfully");
    return true;
}
