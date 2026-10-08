#ifndef CONFIG_H
#define CONFIG_H

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

#include <Arduino.h>
#include <vector>

// Detection constants
#define DEFAULT_PACKET_THRESHOLD 250
#define DEFAULT_CHANNEL_SCAN_TIME_MS 100
#define DEFAULT_CHANNEL_HOP_INTERVAL_MS 75

struct WiFiConfig {
    String sta_ssid;
    String sta_password;
    String admin_user;
    String admin_pass;
};

struct NTPConfig {
    String server;
    int timezone_offset;
    bool daylight_savings;
};

struct DetectionConfig {
    std::vector<String> protected_ssids;
    int reporting_interval_seconds;
    int packet_threshold;
    bool detect_all_deauth;
    int channel_scan_time_ms;
    int channel_hop_interval_ms;
};

// A known/trusted SSID and the BSSID(s) it is allowed to legitimately
// broadcast from. Any other BSSID beaconing this SSID is an evil twin.
struct AllowedNetwork {
    String ssid;
    std::vector<String> bssids;
};

struct WifiIDSConfig {
    bool enabled = false;
    // Beacon-flood / fake-AP-storm: alert when >= beacon_new_bssid_burst
    // distinct new BSSIDs appear within beacon_window_sec; severity escalates
    // to critical when the locally-administered-MAC ratio among them is >=
    // beacon_la_ratio (randomized/spoofed MACs are a strong fake-AP signal).
    int beacon_window_sec = 60;
    int beacon_new_bssid_burst = 6;
    float beacon_la_ratio = 0.6f;
    // KARMA/MANA: alert when one BSSID answers probe-requests for
    // >= karma_distinct_threshold SSIDs it has never actually beaconed,
    // within karma_window_sec.
    int karma_window_sec = 120;
    int karma_distinct_threshold = 2;
    // PNL leak: alert when one client directs probe-requests at
    // >= pnl_distinct_threshold distinct (non-broadcast) SSIDs within
    // pnl_window_sec — it is broadcasting its saved-network list.
    int pnl_window_sec = 120;
    int pnl_distinct_threshold = 3;
    // Per (detector, scope) minimum seconds between repeat alerts, so a
    // sustained attack doesn't flood Telegram with one message per packet.
    int refractory_sec = 300;
    std::vector<AllowedNetwork> allowlist;
};

struct APIConfig {
    String endpoint_url;
    String custom_header_name;
    String custom_header_value;
};

struct HardwareConfig {
    int buzzer_freq;
    int buzzer_duration_ms;
    int screen_brightness;
    bool fancy_intro;
};

struct DebugConfig {
    bool enabled;
};

struct AppConfig {
    WiFiConfig wifi;
    NTPConfig ntp;
    DetectionConfig detection;
    WifiIDSConfig wifi_ids;
    APIConfig api;
    HardwareConfig hardware;
    DebugConfig debug;
};

#endif
