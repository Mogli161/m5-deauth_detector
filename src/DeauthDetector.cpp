#include "DeauthDetector.h"
#include "Logger.h"
#include "WifiIDSDetector.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <WiFi.h>
#include <map>

// Static instance for callback
static DeauthDetector* detectorInstance = nullptr;
static WifiIDSDetector* wifiIdsInstance = nullptr;
static std::map<String, int> ssidPacketCounts;

// Management frame structure
typedef struct {
    unsigned frame_ctrl:16;
    unsigned duration_id:16;
    uint8_t addr1[6]; // receiver address
    uint8_t addr2[6]; // sender address
    uint8_t addr3[6]; // filtering address
    unsigned sequence_ctrl:16;
    uint8_t addr4[6]; // optional
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0];
} wifi_ieee80211_packet_t;

DeauthDetector::DeauthDetector()
    : monitoring(false), currentChannelIndex(0), lastChannelHopTime(0),
      rawHead(0), rawTail(0)
{
    mutex = xSemaphoreCreateMutex();
}

void DeauthDetector::begin(const std::vector<String>& protected_ssids, const DetectionConfig& config) {
    protectedSSIDs = protected_ssids;
    detectionConfig = config;
    detectorInstance = this;
    
    // Discover which channels the protected SSIDs are on
    discoverChannels();
}

void DeauthDetector::discoverChannels() {
    logger.debugPrintln("Discovering channels for protected SSIDs...");
    activeChannels.clear();
    ssidChannelMap.clear();
    bssidToSsidMap.clear();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    // Note: LED control will be added from main.cpp before calling this method
    
    for (int channel = 1; channel <= 14; channel++) {
        int n = WiFi.scanNetworks(false, true, false, detectionConfig.channel_scan_time_ms, channel);
        
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            String bssid = WiFi.BSSIDstr(i);

            // Always store BSSID→SSID for later lookup
            if (!ssid.isEmpty()) {
                bssidToSsidMap[bssid] = ssid;
            }

            for (const String& protected_ssid : protectedSSIDs) {
                if (ssid == protected_ssid) {
                    // Store the SSID to channel mapping
                    ssidChannelMap[ssid] = channel;
                    
                    bool found = false;
                    for (int ch : activeChannels) {
                        if (ch == channel) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        activeChannels.push_back(channel);
                    }
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Found '%s' on channel %d", ssid.c_str(), channel);
                    logger.debugPrintln(buf);
                }
            }
        }
    }
    
    if (activeChannels.empty()) {
        logger.debugPrintln("Warning: No protected SSIDs found.");
        if (detectionConfig.detect_all_deauth) {
            logger.debugPrintln("detect_all_deauth enabled: Monitoring all channels.");
            for (int i = 1; i <= 14; i++) {
                activeChannels.push_back(i);
            }
        } else {
            logger.debugPrintln("detect_all_deauth disabled: No channels to monitor.");
        }
    }
    
    String channelList = "Active channels: ";
    for (int ch : activeChannels) {
        channelList += String(ch) + " ";
    }
    logger.debugPrintln(channelList);
}

void DeauthDetector::startMonitoring() {
    if (monitoring) return;
    
    logger.debugPrintln("Starting packet monitoring...");
    
    WiFi.disconnect();
    delay(100);
    
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&DeauthDetector::packetHandler);
    
    // Initialize channel hopping
    currentChannelIndex = 0;
    lastChannelHopTime = millis();
    if (!activeChannels.empty()) {
        esp_wifi_set_channel(activeChannels[0], WIFI_SECOND_CHAN_NONE);
    }
    
    monitoring = true;
}

void DeauthDetector::stopMonitoring() {
    if (!monitoring) return;
    
    logger.debugPrintln("Stopping packet monitoring...");
    
    esp_wifi_set_promiscuous(false);
    monitoring = false;
}

void DeauthDetector::updateChannelHop() {
    if (!monitoring || activeChannels.empty()) return;

    // Drain ISR ring buffer into events (main-loop context)
    processRawEvents();
    
    unsigned long currentTime = millis();
    if (currentTime - lastChannelHopTime >= detectionConfig.channel_hop_interval_ms) {
        currentChannelIndex = (currentChannelIndex + 1) % activeChannels.size();
        esp_wifi_set_channel(activeChannels[currentChannelIndex], WIFI_SECOND_CHAN_NONE);
        lastChannelHopTime = currentTime;
    }
}

void DeauthDetector::packetHandler(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!detectorInstance || type != WIFI_PKT_MGMT) return;
    
    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const wifi_ieee80211_packet_t* ipkt = (wifi_ieee80211_packet_t*)pkt->payload;
    const wifi_ieee80211_mac_hdr_t* hdr = &ipkt->hdr;
    
    // Extract frame type (bits 3:2) and subtype (bits 7:4)
    uint8_t frameType    = (hdr->frame_ctrl >> 2) & 0x03;
    uint8_t frameSubtype = (hdr->frame_ctrl >> 4) & 0x0F;
    
    // Deauth = Management (type 0x00), subtype 0x0C
    if (frameType == 0x00 && frameSubtype == 0x0C) {
        // Store raw capture into lock-free ring buffer (single-producer from WiFi task)
        size_t nextHead = (detectorInstance->rawHead + 1) % RAW_RING_SIZE;
        if (nextHead == detectorInstance->rawTail) {
            return; // Ring full — drop oldest-overwrite not safe without lock; just drop
        }

        RawDeauthCapture& cap = detectorInstance->rawRing[detectorInstance->rawHead];
        memcpy(cap.addr2, hdr->addr2, 6);
        memcpy(cap.addr3, hdr->addr3, 6);
        cap.channel   = pkt->rx_ctrl.channel;
        cap.rssi      = pkt->rx_ctrl.rssi;
        cap.timestamp  = time(nullptr);

        detectorInstance->rawHead = nextHead;
    } else if (wifiIdsInstance && frameType == 0x00 &&
               (frameSubtype == 0x08 || frameSubtype == 0x05 || frameSubtype == 0x04)) {
        // Beacon (0x08) / probe-response (0x05) / probe-request (0x04) —
        // forward the tagged-parameters region to the WifiIDS detector for
        // beacon-flood/evil-twin/KARMA/PNL-leak analysis. Fixed params before
        // the IEs are 12 bytes for beacon/probe-resp (timestamp+interval+
        // capability), 0 for probe-req (which has no fixed fields).
        size_t fixedParamLen = (frameSubtype == 0x04) ? 0 : 12;
        size_t totalLen = pkt->rx_ctrl.sig_len;
        size_t hdrLen = sizeof(wifi_ieee80211_mac_hdr_t);
        if (totalLen > hdrLen + fixedParamLen) {
            size_t bodyLen = totalLen - hdrLen;
            const uint8_t* ie = ipkt->payload + fixedParamLen;
            size_t ieLen = bodyLen - fixedParamLen;
            // Sanity clamp: never trust sig_len beyond a generous cap, the
            // 802.11 management frame body realistically never exceeds this.
            if (ieLen > 1024) ieLen = 1024;
            wifiIdsInstance->onManagementFrame(frameSubtype, hdr->addr2, hdr->addr3,
                                                pkt->rx_ctrl.channel, pkt->rx_ctrl.rssi,
                                                ie, ieLen);
        }
    }
}

void DeauthDetector::attachWifiIDS(WifiIDSDetector* ids) {
    wifiIdsInstance = ids;
}

void DeauthDetector::processRawEvents() {
    if (rawHead == rawTail) return;  // nothing to drain

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

    while (rawTail != rawHead) {
        const RawDeauthCapture& cap = rawRing[rawTail];
        rawTail = (rawTail + 1) % RAW_RING_SIZE;

        // Format MAC strings
        char bssid[18];
        snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                cap.addr3[0], cap.addr3[1], cap.addr3[2],
                cap.addr3[3], cap.addr3[4], cap.addr3[5]);

        char sender[18];
        snprintf(sender, sizeof(sender), "%02X:%02X:%02X:%02X:%02X:%02X",
                cap.addr2[0], cap.addr2[1], cap.addr2[2],
                cap.addr2[3], cap.addr2[4], cap.addr2[5]);

        String bssidStr = String(bssid);
        String senderStr = String(sender);

        // Per-BSSID packet threshold
        if (ssidPacketCounts[bssidStr] >= detectionConfig.packet_threshold) {
            continue;  // threshold reached for this BSSID
        }
        ssidPacketCounts[bssidStr]++;

        // BSSID → SSID lookup
        String ssidName = "Unknown";
        auto it = bssidToSsidMap.find(bssidStr);
        if (it != bssidToSsidMap.end()) {
            ssidName = it->second;
        }

        DeauthEvent event;
        event.timestamp    = cap.timestamp;
        event.target_ssid  = ssidName;
        event.target_bssid = bssidStr;
        event.attacker_mac = senderStr;
        event.channel      = cap.channel;
        event.rssi         = cap.rssi;
        event.packet_count = ssidPacketCounts[bssidStr];

        events.push_back(event);

        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "Deauth detected: BSSID=%s, Sender=%s, Ch=%d, RSSI=%d",
                 bssid, sender, cap.channel, cap.rssi);
        logger.debugPrintln(logBuf);
    }

    xSemaphoreGive(mutex);
}

bool DeauthDetector::hasEvents() {
    bool result = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        result = !events.empty();
        xSemaphoreGive(mutex);
    }
    return result;
}

std::vector<DeauthEvent> DeauthDetector::getEvents() {
    std::vector<DeauthEvent> copy;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        copy = events;
        xSemaphoreGive(mutex);
    }
    return copy;
}
int DeauthDetector::getChannelForSSID(const String& ssid) {
    // Check stored mapping from startup discovery
    auto it = ssidChannelMap.find(ssid);
    if (it != ssidChannelMap.end()) {
        return it->second;
    }
    
    // If not found in map, check if we have it from recent events
    for (const DeauthEvent& event : events) {
        if (event.target_ssid == ssid && event.channel > 0) {
            return event.channel;
        }
    }
    
    return 0; // Not found
}
void DeauthDetector::clearEvents() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        events.clear();
        ssidPacketCounts.clear();
        xSemaphoreGive(mutex);
    }
}

int DeauthDetector::getEventCountForSSID(const String& ssid) {
    int count = 0;
    for (const DeauthEvent& event : events) {
        if (event.target_ssid == ssid) {
            count++;
        }
    }
    return count;
}

DeauthEvent DeauthDetector::getLastEventForSSID(const String& ssid) {
    for (int i = events.size() - 1; i >= 0; i--) {
        if (events[i].target_ssid == ssid) {
            return events[i];
        }
    }
    return DeauthEvent(); // Return empty event if not found
}

bool DeauthDetector::isProtectedSSID(const String& ssid) {
    for (const String& protected_ssid : protectedSSIDs) {
        if (ssid == protected_ssid) {
            return true;
        }
    }
    return false;
}
