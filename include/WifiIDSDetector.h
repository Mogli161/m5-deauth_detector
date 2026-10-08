#ifndef WIFI_IDS_DETECTOR_H
#define WIFI_IDS_DETECTOR_H

#include <Arduino.h>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <freertos/semphr.h>
#include "Config.h"

// Lightweight POD captured in ISR context — no heap allocations here. A
// bounded raw copy of the tagged-parameters (IE) region is taken so SSID
// extraction (which needs to walk variable-length tags) happens off-ISR,
// in processCapture(), mirroring DeauthDetector's raw-ring-buffer pattern.
static constexpr size_t WIFI_IDS_IE_CAP = 96;

struct RawMgmtCapture {
    uint8_t subtype;       // 4=probe-req, 5=probe-resp, 8=beacon
    uint8_t addr2[6];       // transmitter (station, for probe-req; AP, else)
    uint8_t addr3[6];       // BSSID
    int channel;
    int rssi;
    time_t timestamp;
    uint8_t ie[WIFI_IDS_IE_CAP];
    uint8_t ieLen;
};

static constexpr size_t MGMT_RING_SIZE = 48;

struct WifiIDSEvent {
    time_t timestamp;
    String detector;   // "beacon_flood" | "evil_twin" | "karma" | "pnl_leak"
    String severity;    // "info" | "warning" | "critical"
    String ssid;
    String bssid;
    int channel;
    int rssi;
    String summary;
};

// Passive 802.11 WiFi-IDS: beacon-flood/fake-AP, evil-twin, KARMA/MANA
// rogue-AP, and PNL-leak detection. RX-only, never transmits — same
// monitor-mode promiscuous capture the deauth detector already uses, just
// fed additional management-frame subtypes (beacon/probe-req/probe-resp).
// Ported from Ragnar's wifiwatch.py (passive 802.11 IDS on the Pi watchtower)
// down to a firmware-sized subset: beacon-flood + evil-twin + KARMA + PNL
// leak. EAPOL/PMKID handshake-capture and WPA3-downgrade detection were left
// out of scope (would need a parallel data-frame capture path).
class WifiIDSDetector {
public:
    WifiIDSDetector();
    void begin(const WifiIDSConfig& config);

    // Called from DeauthDetector::packetHandler (the single registered
    // promiscuous RX callback) for beacon/probe-req/probe-resp frames only —
    // ESP32 allows exactly one esp_wifi_set_promiscuous_rx_cb, so this
    // detector is fed via the deauth detector's callback rather than
    // registering a second one.
    void onManagementFrame(uint8_t subtype, const uint8_t* addr2, const uint8_t* addr3,
                            int channel, int rssi, const uint8_t* iePtr, size_t ieLen);

    void update(); // drain the ring buffer and run detection logic; call from main loop
    bool hasEvents();
    std::vector<WifiIDSEvent> getEvents();
    void clearEvents();

private:
    WifiIDSConfig cfg;
    bool enabled;

    SemaphoreHandle_t mutex;
    RawMgmtCapture ring[MGMT_RING_SIZE];
    volatile size_t head; // next write position (ISR)
    volatile size_t tail; // next read position (main loop)

    std::vector<WifiIDSEvent> events;

    // -- beacon flood + evil twin state --
    std::map<String, std::set<String>> apSsids;              // bssid -> ssids beaconed
    std::map<String, unsigned long> firstSeenBssid;           // bssid -> first-seen ms
    std::deque<std::pair<unsigned long, String>> newBssidBurst; // (ms, bssid)
    std::set<String> alertedMultiBssidSsid;                   // dedupe info-level unlisted-multi-bssid

    // -- KARMA/MANA state --
    std::map<String, std::set<String>> probeRespSsids;        // bssid -> ssids answered via probe-resp

    // -- PNL leak state --
    std::map<String, std::deque<std::pair<unsigned long, String>>> clientProbes; // station -> (ms, ssid)
    std::set<String> alertedPnlStation;

    std::map<String, unsigned long> refractory; // "<detector>|<scope>" -> last-fire ms

    void processCapture(const RawMgmtCapture& cap);
    static String extractSSID(const uint8_t* ie, size_t len);
    static bool isLocallyAdministered(const uint8_t* mac);
    static String macToStr(const uint8_t* m);
    bool fire(const String& detector, const String& scope, const String& severity,
              const String& ssid, const String& bssid, int channel, int rssi,
              const String& summary, unsigned long nowMs);
    void pruneMaps();
};

#endif
