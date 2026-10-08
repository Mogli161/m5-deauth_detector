#ifndef DEAUTH_DETECTOR_H
#define DEAUTH_DETECTOR_H

#include <Arduino.h>
#include <vector>
#include <map>
#include <freertos/semphr.h>
#include "Config.h"

class WifiIDSDetector; // fwd decl, see attachWifiIDS()

// Lightweight POD captured in ISR context — no heap allocations
struct RawDeauthCapture {
    uint8_t addr2[6];   // sender MAC
    uint8_t addr3[6];   // BSSID
    int     channel;
    int     rssi;
    time_t  timestamp;
};

static constexpr size_t RAW_RING_SIZE = 64;

struct DeauthEvent {
    time_t timestamp;
    String target_ssid;
    String target_bssid;
    String attacker_mac;
    int channel;
    int rssi;
    int packet_count;
};

class DeauthDetector {
public:
    DeauthDetector();
    void begin(const std::vector<String>& protected_ssids, const DetectionConfig& config);
    void startMonitoring();
    void stopMonitoring();
    bool hasEvents();
    std::vector<DeauthEvent> getEvents();
    void clearEvents();
    int getEventCountForSSID(const String& ssid);
    DeauthEvent getLastEventForSSID(const String& ssid);
    int getChannelForSSID(const String& ssid);
    void updateChannelHop();
    bool isMonitoring() { return monitoring; }

    // Registers a WifiIDSDetector to also receive beacon/probe-req/probe-resp
    // frames from the single shared promiscuous RX callback (ESP32 only
    // allows one esp_wifi_set_promiscuous_rx_cb registration at a time, so
    // this is how the beacon-flood/evil-twin/KARMA/PNL-leak detector gets
    // its frames instead of registering its own).
    static void attachWifiIDS(WifiIDSDetector* ids);

private:
    std::vector<String> protectedSSIDs;
    std::vector<DeauthEvent> events;
    std::vector<int> activeChannels;
    std::map<String, int> ssidChannelMap;
    std::map<String, String> bssidToSsidMap;  // BSSID -> SSID lookup
    bool monitoring;
    DetectionConfig detectionConfig;
    int currentChannelIndex;
    unsigned long lastChannelHopTime;

    // Thread-safe ring buffer for raw captures from ISR
    SemaphoreHandle_t mutex;
    RawDeauthCapture rawRing[RAW_RING_SIZE];
    volatile size_t rawHead;  // next write position (ISR)
    volatile size_t rawTail;  // next read position (main loop)

    void discoverChannels();
    void processRawEvents();
    static void packetHandler(void* buf, wifi_promiscuous_pkt_type_t type);
    bool isProtectedSSID(const String& ssid);
};

#endif
