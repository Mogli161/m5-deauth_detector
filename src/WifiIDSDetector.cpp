#include "WifiIDSDetector.h"
#include "Logger.h"
#include <climits>

WifiIDSDetector::WifiIDSDetector() : enabled(false), head(0), tail(0) {
    mutex = xSemaphoreCreateMutex();
}

void WifiIDSDetector::begin(const WifiIDSConfig& config) {
    cfg = config;
    enabled = config.enabled;
    if (enabled) {
        logger.debugPrintln("WifiIDSDetector: enabled (beacon-flood/evil-twin/karma/pnl-leak)");
    }
}

String WifiIDSDetector::macToStr(const uint8_t* m) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(buf);
}

bool WifiIDSDetector::isLocallyAdministered(const uint8_t* mac) {
    return (mac[0] & 0x02) != 0;
}

// Walk tagged parameters (IEs) for the SSID tag (id 0). Returns "" both for
// a hidden/zero-length SSID and for "no SSID tag found at all" — callers
// treat both the same way (nothing useful to key detection off of).
String WifiIDSDetector::extractSSID(const uint8_t* ie, size_t len) {
    size_t i = 0;
    while (i + 2 <= len) {
        uint8_t eid = ie[i];
        uint8_t elen = ie[i + 1];
        i += 2;
        if (i + elen > len) break;
        if (eid == 0) {
            if (elen == 0) return String("");
            char buf[33];
            size_t n = elen > 32 ? 32 : elen;
            memcpy(buf, ie + i, n);
            buf[n] = 0;
            return String(buf);
        }
        i += elen;
    }
    return String("");
}

void WifiIDSDetector::onManagementFrame(uint8_t subtype, const uint8_t* addr2, const uint8_t* addr3,
                                         int channel, int rssi, const uint8_t* iePtr, size_t ieLen) {
    if (!enabled) return;
    size_t nextHead = (head + 1) % MGMT_RING_SIZE;
    if (nextHead == tail) {
        return; // ring full — drop, same policy as DeauthDetector
    }
    RawMgmtCapture& cap = ring[head];
    cap.subtype = subtype;
    memcpy(cap.addr2, addr2, 6);
    memcpy(cap.addr3, addr3, 6);
    cap.channel = channel;
    cap.rssi = rssi;
    cap.timestamp = time(nullptr);
    size_t copyLen = ieLen > WIFI_IDS_IE_CAP ? WIFI_IDS_IE_CAP : ieLen;
    memcpy(cap.ie, iePtr, copyLen);
    cap.ieLen = (uint8_t)copyLen;
    head = nextHead;
}

bool WifiIDSDetector::fire(const String& detector, const String& scope, const String& severity,
                            const String& ssid, const String& bssid, int channel, int rssi,
                            const String& summary, unsigned long nowMs) {
    String key = detector + "|" + scope;
    auto it = refractory.find(key);
    unsigned long refractoryMs = (unsigned long)cfg.refractory_sec * 1000UL;
    if (it != refractory.end() && (nowMs - it->second) < refractoryMs) {
        return false; // still within the refractory window for this (detector, scope)
    }
    refractory[key] = nowMs;

    WifiIDSEvent ev;
    ev.timestamp = time(nullptr);
    ev.detector = detector;
    ev.severity = severity;
    ev.ssid = ssid;
    ev.bssid = bssid;
    ev.channel = channel;
    ev.rssi = rssi;
    ev.summary = summary;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        events.push_back(ev);
        xSemaphoreGive(mutex);
    }

    char logbuf[180];
    snprintf(logbuf, sizeof(logbuf), "[WifiIDS] %s (%s): %s",
             detector.c_str(), severity.c_str(), summary.c_str());
    logger.debugPrintln(logbuf);
    return true;
}

// Bounded eviction so long uptimes (and an actual beacon-flood, which by
// definition creates many new BSSIDs) can't grow these maps unbounded on a
// ~320KB-SRAM device. Simple oldest-first eviction — good enough for a
// homelab sensor, not trying to be a perfect LRU.
void WifiIDSDetector::pruneMaps() {
    const size_t MAX_TRACKED_BSSIDS = 96;
    if (apSsids.size() > MAX_TRACKED_BSSIDS) {
        String oldestBssid;
        unsigned long oldestTs = ULONG_MAX;
        for (auto& kv : firstSeenBssid) {
            if (kv.second < oldestTs) {
                oldestTs = kv.second;
                oldestBssid = kv.first;
            }
        }
        if (oldestBssid.length()) {
            apSsids.erase(oldestBssid);
            firstSeenBssid.erase(oldestBssid);
            probeRespSsids.erase(oldestBssid);
        }
    }
    const size_t MAX_TRACKED_STATIONS = 64;
    if (clientProbes.size() > MAX_TRACKED_STATIONS) {
        clientProbes.erase(clientProbes.begin());
    }
}

void WifiIDSDetector::processCapture(const RawMgmtCapture& cap) {
    unsigned long nowMs = millis();
    unsigned long windowMs;
    String bssid = macToStr(cap.addr3);
    String station = macToStr(cap.addr2);
    String ssid = extractSSID(cap.ie, cap.ieLen);

    if (cap.subtype == 8) {
        // ---- beacon: feeds both beacon-flood and evil-twin ----
        if (ssid.length() > 0) {
            apSsids[bssid].insert(ssid);
        }
        if (firstSeenBssid.find(bssid) == firstSeenBssid.end()) {
            firstSeenBssid[bssid] = nowMs;
            newBssidBurst.push_back({nowMs, bssid});
        }
        windowMs = (unsigned long)cfg.beacon_window_sec * 1000UL;
        while (!newBssidBurst.empty() && (nowMs - newBssidBurst.front().first) > windowMs) {
            newBssidBurst.pop_front();
        }

        // -- beacon flood / fake-AP storm --
        std::set<String> uniqueInWindow;
        for (auto& e : newBssidBurst) uniqueInWindow.insert(e.second);
        if ((int)uniqueInWindow.size() >= cfg.beacon_new_bssid_burst) {
            int laCount = 0;
            for (auto& b : uniqueInWindow) {
                // First octet of the BSSID string is two hex chars, e.g. "02:..".
                unsigned int firstByte = strtoul(b.substring(0, 2).c_str(), nullptr, 16);
                if (firstByte & 0x02) laCount++;
            }
            float laRatio = uniqueInWindow.empty() ? 0.0f : (float)laCount / uniqueInWindow.size();
            String sev = laRatio >= cfg.beacon_la_ratio ? "critical" : "warning";
            char buf[140];
            snprintf(buf, sizeof(buf),
                     "%d new BSSIDs in %ds (LA ratio %.0f%%) - possible fake-AP storm",
                     (int)uniqueInWindow.size(), cfg.beacon_window_sec, laRatio * 100);
            fire("beacon_flood", "segment", sev, ssid, bssid, cap.channel, cap.rssi, String(buf), nowMs);
        }

        // -- evil twin --
        if (ssid.length() > 0) {
            bool allowlisted = false;
            bool bssidTrusted = false;
            for (auto& net : cfg.allowlist) {
                if (net.ssid.equalsIgnoreCase(ssid)) {
                    allowlisted = true;
                    for (auto& b : net.bssids) {
                        if (b.equalsIgnoreCase(bssid)) { bssidTrusted = true; break; }
                    }
                    break;
                }
            }
            if (allowlisted && !bssidTrusted) {
                char buf[140];
                snprintf(buf, sizeof(buf),
                         "SSID '%s' beaconed from un-allowlisted BSSID %s (evil twin)",
                         ssid.c_str(), bssid.c_str());
                fire("evil_twin", ssid, "critical", ssid, bssid, cap.channel, cap.rssi, String(buf), nowMs);
            } else if (!allowlisted) {
                int distinctBssids = 0;
                for (auto& kv : apSsids) {
                    if (kv.second.count(ssid)) distinctBssids++;
                }
                if (distinctBssids >= 2 && alertedMultiBssidSsid.find(ssid) == alertedMultiBssidSsid.end()) {
                    alertedMultiBssidSsid.insert(ssid);
                    char buf[140];
                    snprintf(buf, sizeof(buf),
                             "SSID '%s' seen from %d BSSIDs (unlisted; add to allowlist to confirm)",
                             ssid.c_str(), distinctBssids);
                    fire("evil_twin", ssid, "info", ssid, bssid, cap.channel, cap.rssi, String(buf), nowMs);
                }
            }
        }
    } else if (cap.subtype == 5) {
        // ---- probe-response: KARMA/MANA ----
        if (ssid.length() > 0 && apSsids[bssid].count(ssid) == 0) {
            // This BSSID answered for an SSID it has never itself beaconed.
            probeRespSsids[bssid].insert(ssid);
            int distinct = probeRespSsids[bssid].size();
            if (distinct >= cfg.karma_distinct_threshold) {
                char buf[140];
                snprintf(buf, sizeof(buf),
                         "BSSID %s answered %d SSID(s) it never beacons (KARMA/MANA rogue AP)",
                         bssid.c_str(), distinct);
                fire("karma", bssid, "critical", ssid, bssid, cap.channel, cap.rssi, String(buf), nowMs);
            }
        }
    } else if (cap.subtype == 4) {
        // ---- probe-request: PNL leak ----
        if (ssid.length() > 0) {
            auto& dq = clientProbes[station];
            dq.push_back({nowMs, ssid});
            windowMs = (unsigned long)cfg.pnl_window_sec * 1000UL;
            while (!dq.empty() && (nowMs - dq.front().first) > windowMs) dq.pop_front();
            std::set<String> distinctSsids;
            for (auto& e : dq) distinctSsids.insert(e.second);
            if ((int)distinctSsids.size() >= cfg.pnl_distinct_threshold &&
                alertedPnlStation.find(station) == alertedPnlStation.end()) {
                alertedPnlStation.insert(station);
                char buf[140];
                snprintf(buf, sizeof(buf),
                         "Client %s leaked %d saved SSID(s) via directed probes (PNL leak)",
                         station.c_str(), (int)distinctSsids.size());
                fire("pnl_leak", station, "warning", ssid, bssid, cap.channel, cap.rssi, String(buf), nowMs);
            }
        }
    }

    pruneMaps();
}

void WifiIDSDetector::update() {
    if (!enabled) return;
    if (head == tail) return;

    // Drain into a local snapshot quickly, then process outside the ring
    // index critical section (processCapture only touches the detector's
    // own non-ISR-shared state, so this split just keeps the ring-pointer
    // bookkeeping tight).
    while (tail != head) {
        RawMgmtCapture cap = ring[tail];
        tail = (tail + 1) % MGMT_RING_SIZE;
        processCapture(cap);
    }
}

bool WifiIDSDetector::hasEvents() {
    bool r = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        r = !events.empty();
        xSemaphoreGive(mutex);
    }
    return r;
}

std::vector<WifiIDSEvent> WifiIDSDetector::getEvents() {
    std::vector<WifiIDSEvent> copy;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        copy = events;
        xSemaphoreGive(mutex);
    }
    return copy;
}

void WifiIDSDetector::clearEvents() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        events.clear();
        xSemaphoreGive(mutex);
    }
}
