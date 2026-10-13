# Data-Driven Detector Menu, Per-Module Stats & Overview Dashboard

**For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Replace the hardcoded 4-entry menu (Dashboard / Live Log / Detailed / Gif) with a data-driven menu: `Overview` as entry 0, then one entry per detector that can actually fire, each with a per-detector stats screen.

**Architecture:** A `DetectorModule` abstraction (function-pointer interface) in a new header; thin adapters in `main.cpp` implement it per detector; a registry filtered by `canFire()` holds the active list. `Display` reads that list to build the menu and render stats — no detector-specific branches, no boot counters (session counts only).

**Tech Stack:** C++ / Arduino (ESP32-S3, M5Cardputer, PlatformIO), M5Cardputer display API (240×135, rotation 1), existing `DeauthDetector` + `WifiIDSDetector`.

**PlatformIO env:** `m5stack-stamps3` (the only env in `platformio.ini`). All builds: `pio run -e m5stack-stamps3`.

**Reference:** [Issue #1](https://github.com/Mogli161/m5-deauth_detector/issues/1), `TODO.md`

**Decisions locked in:** Overview = menu entry 0 · session counts only (no boot counters) · `Detailed`/SSID paging retained · only detectors that can actually fire get a menu entry.

---

## Findings that shape this plan

### Which detectors are actually installed (verified in code)

All five are implemented and wired — not four:

| Module | `fire()` call site | Can it actually fire? |
|---|---|---|
| `deauth` | `DeauthDetector.cpp` | Yes — needs `protected_ssids` non-empty, or `detect_all_deauth` |
| `beacon_flood` | `WifiIDSDetector.cpp:168` | Yes — thresholds only (`beacon_new_bssid_burst` within `beacon_window_sec`) |
| `evil_twin` | `:189` (critical), `:201` (info) | **Only partially — see caveat below** |
| `karma` | `:216` | Yes — `karma_distinct_threshold` |
| `pnl_leak` | `:235` | Yes — `pnl_distinct_threshold` |

Wiring is complete end to end: `packetHandler` forwards beacon/probe-resp/probe-req frames to the IDS detector (`DeauthDetector.cpp:181-205`), and `wifiIdsDetector.begin()` / `.update()` / `attachWifiIDS()` are all called in `main.cpp` (167, 242, 168). Nothing is stubbed out or dead.

**Two caveats the user should know:**

1. **The four IDS sub-detectors are not independently switchable.** There is exactly one flag, `wifi_ids.enabled` (`Config.h:46`). `WifiIDSConfig` has no per-detector toggles. They are all on or all off.

2. **`evil_twin` is inert at its real severity without an allowlist.** Its *critical* path (`:183-189`) only fires when the SSID **is** in `cfg.allowlist` and the BSSID is not trusted. With an empty allowlist it falls through to the *info* path (`:190-201`), which only reports *"SSID seen from N BSSIDs, add to allowlist to confirm"* — a hint, not a detection. So on a device with no allowlist, Evil-Twin produces noise, not alerts.

**Limitation:** the live `config.txt` is gitignored and lives only on the Cardputer, so **which detectors are enabled on the running device cannot be determined from the repo**. The menu must therefore derive from runtime state (`canFire` / `isArmed` / counts) rather than a hardcoded list — that is precisely what `canFire()` in Task 2 provides.

---

## Task 1: Define the `DetectorModule` abstraction

**Objective:** One interface so `Display` treats every detector uniformly.

**Files:**
- Create: `include/DetectorModule.h`

**Step 1: Write the header**

```cpp
#ifndef DETECTOR_MODULE_H
#define DETECTOR_MODULE_H

#include <Arduino.h>
#include <vector>
#include <cstdint>

// One row in a per-module event list. Copyable POD-ish struct; built only
// on the main-loop thread (String temporaries), never in ISR context.
struct ModuleEvent {
    time_t timestamp = 0;
    String  scope;      // SSID (deauth) or BSSID/station (IDS detectors)
    String  detail;     // attacker MAC (deauth) or summary (IDS)
    int     channel = 0;
    int     rssi = 0;
    int     packetCount = 1;
    String  severity;   // IDS only; empty for deauth
};

// A detector as the display sees it. Implemented by thin adapters in
// main.cpp, one per detection module, so Display.cpp needs no
// detector-specific branches. Adding a detector = one adapter + one
// registry entry; Display stays untouched.
struct DetectorModule {
    const char* name;       // menu label, e.g. "Deauth", "Beacon-Flood"
    const char* id;         // stable key, e.g. "deauth", "beacon_flood"
    uint32_t    ledColor;   // 0xRRGGBB, matches the alert LED color

    // False when this detector cannot fire at all (its config block is off,
    // or it needs config the user has not supplied). Such modules get no
    // menu entry.
    bool (*canFire)(void* ctx);
    // True while actively capturing.
    bool (*isArmed)(void* ctx);

    // Session-scoped event count (reset by clearEvents()).
    int    (*eventCount)(void* ctx);
    time_t (*lastEventTs)(void* ctx);
    bool   (*lastEvent)(void* ctx, ModuleEvent& out);
    void   (*events)(void* ctx, std::vector<ModuleEvent>& out);

    // Optional per-scope breakdown (protected SSIDs for deauth; null for
    // IDS detectors). Lets the stats view page scopes without Display
    // knowing what a "scope" is.
    int         (*scopeCount)(void* ctx);
    const char* (*scopeName)(void* ctx, int i);
    int         (*scopeEventCount)(void* ctx, int i);

    void* ctx;
};

// Safe calls through possibly-null function pointers.
inline bool       modCanFire(const DetectorModule& m)          { return m.canFire ? m.canFire(m.ctx) : false; }
inline bool       modIsArmed(const DetectorModule& m)          { return m.isArmed ? m.isArmed(m.ctx) : false; }
inline int        modEventCount(const DetectorModule& m)       { return m.eventCount ? m.eventCount(m.ctx) : 0; }
inline time_t     modLastEventTs(const DetectorModule& m)      { return m.lastEventTs ? m.lastEventTs(m.ctx) : 0; }
inline bool       modLastEvent(const DetectorModule& m, ModuleEvent& o) { return m.lastEvent ? m.lastEvent(m.ctx, o) : false; }
inline void       modEvents(const DetectorModule& m, std::vector<ModuleEvent>& o) { if (m.events) m.events(m.ctx, o); }
inline int        modScopeCount(const DetectorModule& m)       { return m.scopeCount ? m.scopeCount(m.ctx) : 0; }
inline const char* modScopeName(const DetectorModule& m, int i) { return m.scopeName ? m.scopeName(m.ctx, i) : ""; }
inline int        modScopeEventCount(const DetectorModule& m, int i) { return m.scopeEventCount ? m.scopeEventCount(m.ctx, i) : 0; }

#endif
```

**Step 2: Commit**

```bash
git add include/DetectorModule.h
git commit -m "feat: add DetectorModule abstraction for data-driven menu"
```

---

## Task 2: Adapters + canFire-filtered registry, and the missing `isEnabled()`

**Objective:** Wrap both detectors behind `DetectorModule`; register only modules that can actually fire.

**Files:**
- Modify: `include/WifiIDSDetector.h`
- Modify: `src/main.cpp`

**Step 1: Add the missing accessor**

`WifiIDSDetector::enabled` is private with no getter. Add to the public section of `include/WifiIDSDetector.h`:

```cpp
    bool isEnabled() { return enabled; }
```

**Step 2: Add the deauth adapters** (after `colorForIdsDetector()`, ~line 53)

```cpp
// ---- deauth adapter ----------------------------------------------------
static int deauthEventCount(void*) { return (int)detector.getEvents().size(); }
static time_t deauthLastTs(void*) {
    std::vector<DeauthEvent> ev = detector.getEvents();
    return ev.empty() ? 0 : ev.back().timestamp;
}
static bool deauthLastEvent(void*, ModuleEvent& out) {
    std::vector<DeauthEvent> ev = detector.getEvents();
    if (ev.empty()) return false;
    const DeauthEvent& e = ev.back();
    out.timestamp = e.timestamp;
    out.scope = e.target_ssid;
    out.detail = e.attacker_mac;
    out.channel = e.channel;
    out.rssi = e.rssi;
    out.packetCount = e.packet_count;
    return true;
}
static void deauthEvents(void*, std::vector<ModuleEvent>& out) {
    for (const DeauthEvent& e : detector.getEvents()) {
        ModuleEvent m;
        m.timestamp = e.timestamp;
        m.scope = e.target_ssid;
        m.detail = e.attacker_mac;
        m.channel = e.channel;
        m.rssi = e.rssi;
        m.packetCount = e.packet_count;
        out.push_back(m);
    }
}
static bool deauthCanFire(void*) {
    // Protected SSIDs configured, or catch-all deauth detection on.
    AppConfig& c = configManager.getConfig();
    return !c.detection.protected_ssids.empty() || c.detection.detect_all_deauth;
}
static bool deauthIsArmed(void*) { return detector.isMonitoring(); }

// Deauth scopes = protected SSIDs, so the stats view can page them exactly
// like the old Detailed view did.
static std::vector<String>& deauthScopeSsids() {
    return configManager.getConfig().detection.protected_ssids;
}
static int deauthScopeCount(void*) { return (int)deauthScopeSsids().size(); }
static const char* deauthScopeName(void*, int i) {
    static String buf;
    std::vector<String>& s = deauthScopeSsids();
    buf = (i >= 0 && i < (int)s.size()) ? s[i] : String("");
    return buf.c_str();
}
static int deauthScopeEventCount(void*, int i) {
    std::vector<String>& s = deauthScopeSsids();
    return (i >= 0 && i < (int)s.size()) ? detector.getEventCountForSSID(s[i]) : 0;
}
```

**Step 3: Add the IDS adapters**

Each IDS sub-detector is a *filtered view* over the one shared `WifiIDSDetector` (they share a single promiscuous RX callback), so the adapter filters events by `detector` name.

```cpp
// Index order; must match IDS_MODULES below.
static const char* const IDS_DETECTOR_NAMES[] = {
    "beacon_flood", "evil_twin", "karma", "pnl_leak"
};
static const int IDS_DETECTOR_COUNT = 4;

// Evil-twin's real (critical) path needs an allowlist entry; without one it
// only emits info-level "seen from N BSSIDs" hints. evilTwinCanFire()
// uses this so the menu does not advertise a detection that cannot alert.
static bool idsHasAllowlist() {
    return !configManager.getConfig().wifi_ids.allowlist.empty();
}

struct IdsModuleCtx { int index; };

static std::vector<WifiIDSEvent> idsEventsFor(int index) {
    std::vector<WifiIDSEvent> all = wifiIdsDetector.getEvents();
    std::vector<WifiIDSEvent> filtered;
    for (const WifiIDSEvent& e : all) {
        if (e.detector == IDS_DETECTOR_NAMES[index]) filtered.push_back(e);
    }
    return filtered;
}

static int idsEventCount(void* ctx) { return (int)idsEventsFor(((IdsModuleCtx*)ctx)->index).size(); }
static time_t idsLastTs(void* ctx) {
    std::vector<WifiIDSEvent> ev = idsEventsFor(((IdsModuleCtx*)ctx)->index);
    return ev.empty() ? 0 : ev.back().timestamp;
}
static bool idsLastEvent(void* ctx, ModuleEvent& out) {
    std::vector<WifiIDSEvent> ev = idsEventsFor(((IdsModuleCtx*)ctx)->index);
    if (ev.empty()) return false;
    const WifiIDSEvent& e = ev.back();
    out.timestamp = e.timestamp;
    out.scope = e.ssid;
    out.detail = e.bssid;
    out.channel = e.channel;
    out.rssi = e.rssi;
    out.severity = e.severity;
    return true;
}
static void idsEvents(void* ctx, std::vector<ModuleEvent>& out) {
    for (const WifiIDSEvent& e : idsEventsFor(((IdsModuleCtx*)ctx)->index)) {
        ModuleEvent m;
        m.timestamp = e.timestamp;
        m.scope = e.ssid;
        m.detail = e.bssid;
        m.channel = e.channel;
        m.rssi = e.rssi;
        m.severity = e.severity;
        out.push_back(m);
    }
}
// All four IDS sub-detectors share one enable flag (wifi_ids.enabled) --
// there are no per-detector toggles in WifiIDSConfig.
static bool idsCanFire(void*)      { return wifiIdsDetector.isEnabled(); }
static bool idsIsArmed(void*)      { return wifiIdsDetector.isEnabled(); }
// Exception: evil_twin additionally needs an allowlist to alert for real.
static bool evilTwinCanFire(void*) {
    return wifiIdsDetector.isEnabled() && idsHasAllowlist();
}
```

**Step 4: Build the registry, filtered by `canFire()`**

```cpp
static IdsModuleCtx g_idsCtx[IDS_DETECTOR_COUNT] = {{0}, {1}, {2}, {3}};

// Every detector the firmware knows about. canFire()/isArmed() are
// evaluated every refresh, so a module drops out of the menu automatically
// when its config turns it off -- no hardcoded assumptions about the device.
static DetectorModule g_allModules[] = {
    {"Deauth",       "deauth",       COLOR_DEAUTH,
     deauthCanFire, deauthIsArmed,
     deauthEventCount, deauthLastTs, deauthLastEvent, deauthEvents,
     deauthScopeCount, deauthScopeName, deauthScopeEventCount, nullptr},

    {"Beacon-Flood", "beacon_flood", COLOR_BEACON_FLOOD,
     idsCanFire, idsIsArmed,
     idsEventCount, idsLastTs, idsLastEvent, idsEvents,
     nullptr, nullptr, nullptr, &g_idsCtx[0]},

    {"Evil-Twin",    "evil_twin",    COLOR_EVIL_TWIN,
     evilTwinCanFire, idsIsArmed,
     idsEventCount, idsLastTs, idsLastEvent, idsEvents,
     nullptr, nullptr, nullptr, &g_idsCtx[1]},

    {"KARMA/MANA",   "karma",        COLOR_KARMA,
     idsCanFire, idsIsArmed,
     idsEventCount, idsLastTs, idsLastEvent, idsEvents,
     nullptr, nullptr, nullptr, &g_idsCtx[2]},

    {"PNL-Leak",     "pnl_leak",     COLOR_PNL_LEAK,
     idsCanFire, idsIsArmed,
     idsEventCount, idsLastTs, idsLastEvent, idsEvents,
     nullptr, nullptr, nullptr, &g_idsCtx[3]},
};
static const int ALL_MODULE_COUNT = 5;

// The single source of truth for the menu: only modules that can actually
// fire. Recomputed each refresh so config changes take effect live.
static std::vector<DetectorModule> g_activeModules;

static void refreshActiveModules() {
    g_activeModules.clear();
    for (int i = 0; i < ALL_MODULE_COUNT; i++) {
        if (modCanFire(g_allModules[i])) g_activeModules.push_back(g_allModules[i]);
    }
}

// Accessor for Display and SerialConsole.
std::vector<DetectorModule>& activeDetectorModules() { return g_activeModules; }
```

Declare it near the other forward declarations in `main.cpp` (~line 83):

```cpp
std::vector<DetectorModule>& activeDetectorModules();
```

**Step 5: Refresh the list in the main loop**

In `loop()`, immediately before the `updateDisplay()` call (~line 318):

```cpp
refreshActiveModules();
```

**Step 6: Build and commit**

Run: `pio run -e m5stack-stamps3`
Expected: build succeeds

```bash
git add include/WifiIDSDetector.h src/main.cpp
git commit -m "feat: DetectorModule adapters, canFire-filtered registry, isEnabled accessor"
```

---

## Task 3: Data-driven menu with Overview as entry 0

**Files:**
- Modify: `include/Display.h`
- Modify: `src/Display.cpp:4-5,7,21-29,31-55`

**Step 1: Extend the header**

```cpp
#include <vector>
#include "DetectorModule.h"
```

Enum:

```cpp
enum DisplayView {
    VIEW_MENU,
    VIEW_OVERVIEW,      // menu entry 0: all detectors at a glance
    VIEW_MODULE_STATS,  // per-detector stats, selected from the menu
    VIEW_LIVE_LOG,
    VIEW_DETAILED,      // retained: SSID-paged deauth view
    VIEW_GIF,
    VIEW_CONFIG_INFO
};
```

Public methods:

```cpp
    void setModules(const std::vector<DetectorModule>& mods);
    void showOverview(const std::vector<DetectorModule>& mods);
    void showModuleStats(const DetectorModule& mod);
    void moduleStatsNextScope();
    void moduleStatsPrevScope();
```

Private members:

```cpp
    std::vector<DetectorModule> modules;  // snapshot for rendering
    int moduleStatsPage;                  // scope paging within the open module
```

**Step 2: Replace the static menu array in `Display.cpp`**

Delete lines 4-5 (`MENU_ITEMS`, `MENU_ITEM_COUNT`).

**Step 3: Registry-driven navigation, Overview at index 0**

```cpp
void Display::setModules(const std::vector<DetectorModule>& mods) { modules = mods; }

// +1 for the Overview row at index 0.
int Display::getMenuItemCount() { return (int)modules.size() + 1; }

void Display::menuUp() {
    int n = getMenuItemCount();
    if (n == 0) return;
    menuIndex--;
    if (menuIndex < 0) menuIndex = n - 1;
}

void Display::menuDown() {
    int n = getMenuItemCount();
    if (n == 0) return;
    menuIndex++;
    if (menuIndex >= n) menuIndex = 0;
}

DisplayView Display::menuIndexToView(int index) {
    if (index == 0) return VIEW_OVERVIEW;
    if (index >= 1 && index - 1 < (int)modules.size()) return VIEW_MODULE_STATS;
    return VIEW_MENU;
}
```

**Step 4: Rewrite `showMenu()`**

```cpp
void Display::showMenu() {
    clearScreen();
    drawHeader("Detectors");

    // 135px screen: 20px header + 10px footer -> ~105px for rows.
    // At 15px/row that is 7 rows; Overview + 5 modules = 6, so it fits.
    const int ROW_H = 15;
    int y = 24;

    // Row 0: Overview.
    if (menuIndex == 0) {
        M5Cardputer.Display.fillRect(0, y - 2, 240, ROW_H, 0x39C7);
        M5Cardputer.Display.setTextColor(WHITE, 0x39C7);
    } else {
        M5Cardputer.Display.setTextColor(WHITE, BLACK);
    }
    M5Cardputer.Display.setCursor(4, y);
    M5Cardputer.Display.print("> Overview");
    M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
    M5Cardputer.Display.setCursor(120, y);
    M5Cardputer.Display.print("all");
    y += ROW_H;

    // One row per active detector: color swatch, armed marker, count.
    for (int i = 0; i < (int)modules.size(); i++) {
        const DetectorModule& m = modules[i];
        if (i + 1 == menuIndex) {
            M5Cardputer.Display.fillRect(0, y - 2, 240, ROW_H, 0x39C7);
            M5Cardputer.Display.setTextColor(WHITE, 0x39C7);
        } else {
            M5Cardputer.Display.setTextColor(WHITE, BLACK);
        }
        M5Cardputer.Display.setCursor(4, y);

        uint16_t c = M5Cardputer.Display.color565(
            (m.ledColor >> 16) & 0xFF, (m.ledColor >> 8) & 0xFF, m.ledColor & 0xFF);
        M5Cardputer.Display.fillRect(4, y + 3, 6, 6, c);

        M5Cardputer.Display.setCursor(14, y);
        M5Cardputer.Display.print(modIsArmed(m) ? "* " : "  ");
        M5Cardputer.Display.print(m.name);

        String cnt = String(modEventCount(m));
        M5Cardputer.Display.setCursor(240 - cnt.length() * 6 - 6, y);
        M5Cardputer.Display.print(cnt);
        y += ROW_H;
    }

    M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
    M5Cardputer.Display.setCursor(4, 122);
    M5Cardputer.Display.print("Up/Down sel, Enter open, Esc back");
}
```

**Step 5: Reset scope paging when the view changes**

```cpp
void Display::setView(DisplayView v) {
    if (v == VIEW_MODULE_STATS) moduleStatsPage = 0;
    currentView = v;
}
```

Add `moduleStatsPage(0)` to the constructor initializer list in `Display.cpp:7`.

**Step 6: Build (expect failure), commit WIP**

Run: `pio run -e m5stack-stamps3`
Expected: **FAILS** — `showOverview`/`showModuleStats` declared but undefined. Task 4 fixes it.

```bash
git add include/Display.h src/Display.cpp
git commit -m "feat: data-driven menu, Overview as entry 0, per-module count and LED swatch"
```

---

## Task 4: Overview + per-module stats views

**Files:**
- Modify: `src/Display.cpp` (append after `showDetailed`)

**Step 1: `showOverview()`**

```cpp
void Display::showOverview(const std::vector<DetectorModule>&mods) {
    clearScreen();
    drawHeader("Overview");

    M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
    M5Cardputer.Display.setCursor(6, 22);
    M5Cardputer.Display.print("Detector");
    M5Cardputer.Display.setCursor(120, 22);
    M5Cardputer.Display.print("Evt");
    M5Cardputer.Display.setCursor(165, 22);
    M5Cardputer.Display.print("Last");

    int y = 34;
    time_t now = time(nullptr);
    for (const DetectorModule&m : mods) {
        if (y > 112) break;

        uint16_t c = M5Cardputer.Display.color565(
            (m.ledColor >> 16) & 0xFF, (m.ledColor >> 8) & 0xFF, m.ledColor & 0xFF);
        M5Cardputer.Display.fillRect(4, y + 3, 6, 6, c);

        M5Cardputer.Display.setTextColor(WHITE, BLACK);
        M5Cardputer.Display.setCursor(14, y);
        M5Cardputer.Display.print(String(m.name).substring(0, 16));

        M5Cardputer.Display.setCursor(104, y);
        M5Cardputer.Display.setTextColor(modIsArmed(m) ? GREEN : 0x7BEF, BLACK);
        M5Cardputer.Display.print(modIsArmed(m) ? "ON " : "-- ");

        int cnt = modEventCount(m);
        M5Cardputer.Display.setTextColor(cnt > 0 ? c : WHITE, BLACK);
        M5Cardputer.Display.setCursor(120, y);
        M5Cardputer.Display.print(cnt);

        M5Cardputer.Display.setTextColor(WHITE, BLACK);
        M5Cardputer.Display.setCursor(165, y);
        time_t ts = modLastEventTs(m);
        if (ts == 0) {
            M5Cardputer.Display.print("--");
        } else {
            long age = (long)(now - ts);
            if (age < 60)        M5Cardputer.Display.print(String(age) + "s");
            else if (age < 3600) M5Cardputer.Display.print(String(age / 60) + "m");
            else                 M5Cardputer.Display.print(String(age / 3600) + "h");
        }
        y += 14;
    }

    drawFooter();
}
```

**Step 2: `showModuleStats()` with SSID paging**

```cpp
void Display::showModuleStats(const DetectorModule&mod) {
    clearScreen();

    uint16_t c = M5Cardputer.Display.color565(
        (mod.ledColor >> 16) & 0xFF, (mod.ledColor >> 8) & 0xFF, mod.ledColor & 0xFF);
    M5Cardputer.Display.fillRect(0, 0, 240, 20, c);
    M5Cardputer.Display.setTextColor(BLACK, c);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print(mod.name);

    // --- aggregates (session count only) ---
    M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
    M5Cardputer.Display.setCursor(5, 24);
    M5Cardputer.Display.print("Events:");
    M5Cardputer.Display.setCursor(5, 36);
    M5Cardputer.Display.print("Last:");
    M5Cardputer.Display.setCursor(5, 48);
    M5Cardputer.Display.print("Target:");

    ModuleEvent last;
    bool haveLast = modLastEvent(mod, last);

    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    M5Cardputer.Display.setCursor(80, 24);
    M5Cardputer.Display.print(modEventCount(mod));
    M5Cardputer.Display.setCursor(80, 36);
    M5Cardputer.Display.print(haveLast ? formatDateTime(last.timestamp) : "--");
    M5Cardputer.Display.setCursor(80, 48);
    M5Cardputer.Display.print(haveLast ? last.scope.substring(0, 24) : "--");

    int scopes = modScopeCount(mod);
    if (scopes > 0) {
        // --- deauth: per-SSID breakdown, paged ---
        if (moduleStatsPage >= scopes) moduleStatsPage = 0;
        M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
        M5Cardputer.Display.setCursor(5, 62);
        M5Cardputer.Display.print("SSIDs (n/m page):");
        int y = 74;
        for (int i = 0; i < scopes && y <= 112; i++) {
            bool sel = (i == moduleStatsPage);
            M5Cardputer.Display.setTextColor(sel ? GREEN : WHITE, BLACK);
            M5Cardputer.Display.setCursor(5, y);
            String nm = String(modScopeName(mod, i));
            M5Cardputer.Display.print((sel ? "> " : "  ") + nm.substring(0, 18));
            M5Cardputer.Display.setCursor(170, y);
            M5Cardputer.Display.print(modScopeEventCount(mod, i));
            y += 11;
        }
    } else {
        // --- IDS modules have no scopes: show recent events instead ---
        M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
        M5Cardputer.Display.setCursor(5, 62);
        M5Cardputer.Display.print("Recent:");
        std::vector<ModuleEvent> evs;
        modEvents(mod, evs);
        int y = 74, shown = 0;
        for (int i = (int)evs.size() - 1; i >= 0 && shown < 4; i--, shown++) {
            const ModuleEvent& e = evs[i];
            M5Cardputer.Display.setTextColor(WHITE, BLACK);
            M5Cardputer.Display.setCursor(5, y);
            String line = formatTime(e.timestamp) + " ch" + String(e.channel) +
                          " " + e.scope.substring(0, 12);
            if (!e.severity.isEmpty()) line += " [" + e.severity + "]";
            M5Cardputer.Display.print(line.substring(0, 38));
            y += 11;
        }
        if (shown == 0) {
            M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
            M5Cardputer.Display.setCursor(5, 74);
            M5Cardputer.Display.print("(none yet)");
        }
    }

    M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
    M5Cardputer.Display.setCursor(5, 122);
    M5Cardputer.Display.print(modIsArmed(mod) ? "monitoring" : "not armed");
    M5Cardputer.Display.setCursor(170, 122);
    M5Cardputer.Display.print("Esc=back");
}

void Display::moduleStatsNextScope() { moduleStatsPage++; }
void Display::moduleStatsPrevScope() { if (moduleStatsPage > 0) moduleStatsPage--; }
```

**Step 3: Wire the views into `updateDisplay()` in `main.cpp`**

Replace the `VIEW_DASHBOARD` case (~line 390):

```cpp
void updateDisplay() {
    AppConfig& config = configManager.getConfig();
    std::vector<DeauthEvent> events = detector.getEvents();

    // Registry snapshot for this frame; keeps menu counts current and drops
    // modules whose config turned them off.
    display.setModules(activeDetectorModules());

    switch (display.getCurrentView()) {
        case VIEW_MENU:
            display.showMenu();
            break;

        case VIEW_OVERVIEW:
            display.showOverview(activeDetectorModules());
            break;

        case VIEW_MODULE_STATS: {
            int idx = display.getMenuIndex() - 1;  // index 0 is Overview
            if (idx >= 0 && idx < (int)activeDetectorModules().size()) {
                display.showModuleStats(activeDetectorModules()[idx]);
            } else {
                display.setView(VIEW_MENU);
                display.showMenu();
            }
            break;
        }

        case VIEW_LIVE_LOG:
            display.showLiveLog(events);
            break;

        case VIEW_DETAILED:
            display.showDetailed(config.detection.protected_ssids, detector);
            break;

        case VIEW_GIF:
        case VIEW_CONFIG_INFO:
            // Handled by their own blocking loops.
            break;
    }
}
```

**Step 4: Bind the scope-paging keys**

Verified in `src/main.cpp:353-358`: the existing paging keys are **`n` (left/prev) and `m` (right/next)**, guarded by `!status.word.empty()`. `.` and `;` are already taken by menu up/down, and `` ` `` is Esc. So reuse `n`/`m` for consistency with `Detailed`.

Extend the existing `VIEW_DETAILED` branch (`main.cpp:352-359`) into a paging branch that also serves the stats view:

```cpp
            } else if (view == VIEW_DETAILED || view == VIEW_MODULE_STATS) {
                if (!status.word.empty()) {
                    if (keyStateHasChar(status, 'n')) { // Left arrow
                        if (view == VIEW_DETAILED) {
                            display.prevDetailedPage(config.detection.protected_ssids.size());
                        } else {
                            display.moduleStatsPrevScope();
                        }
                        updateDisplay();
                    } else if (keyStateHasChar(status, 'm')) { // Right arrow
                        if (view == VIEW_DETAILED) {
                            display.nextDetailedPage(config.detection.protected_ssids.size());
                        } else {
                            display.moduleStatsNextScope();
                        }
                        updateDisplay();
                    }
                }
            }
```

Key map for reference (all verified in `main.cpp:67-70` and the key handler): `` ` `` = Esc/back · `;` = menu up · `.` = menu down · `n`/`m` = prev/next page.

**Step 5: Build and commit**

Run: `pio run -e m5stack-stamps3`
Expected: build succeeds

```bash
git add src/Display.cpp src/main.cpp
git commit -m "feat: overview dashboard and per-detector stats with SSID paging"
```

---

## Task 5: `VIEW_DETAILED` stays as-is

`Detailed` remains the SSID-paged deauth view. It is **not** removed and **not** re-pointed — per-module stats (Task 4) now cover per-detector breakdowns, while `Detailed` keeps the paged per-SSID deauth view users already know. No code change in this task.

Confirm the build still passes after the menu refactor:

Run: `pio run -e m5stack-stamps3`
Expected: build succeeds

```bash
git commit --allow-empty -m "docs: Detailed view retained alongside per-module stats"
```

---

## Task 6: Serial console parity

**Files:**
- Modify: `src/SerialConsole.cpp`

**Step 1: Inspect existing commands**

```bash
grep -n '"dashboard"\|"log"\|"detailed"\|case "' src/SerialConsole.cpp
```

**Step 2: Add module-aware commands**

`modules` lists every active module with armed state and count; `module <id>` dumps one module's stats as text. Both read `activeDetectorModules()` (added in Task 2), so no per-detector serial code is needed.

**Step 3: Build and commit**

```bash
git commit -m "feat: serial console modules/module commands"
```

---

## Task 7: Docs + close the issue

**Files:**
- Modify: `README.md`
- Modify: `TODO.md`

**Step 1: Document in `README.md`**

- Menu layout: `Overview` at index 0, then one row per active detector
- Stats fields per module (session count, last timestamp, last target, per-SSID paging for deauth)
- **How to add a detector:** one adapter + one `g_allModules[]` entry; `Display` needs no change
- **The `evil_twin` caveat:** without an allowlist entry it only emits info-level hints, and therefore gets no menu entry (by design, via `evilTwinCanFire`)
- **The single-flag caveat:** the four IDS sub-detectors share `wifi_ids.enabled`; they cannot be toggled individually

**Step 2: Empty `TODO.md`** — replace the body with a pointer to the closed issue, or delete the file.

**Step 3: Close the issue**

```bash
gh issue close 1 --repo Mogli161/m5-deauth_detector \
  --comment "Implemented: data-driven detector menu (Overview + one entry per active detector), per-module stats with SSID paging. See README."
```

**Step 4: Commit and push**

```bash
git add README.md TODO.md
git commit -m "docs: document data-driven detector menu and module stats"
git push origin main
```

---

## Verification checklist

On real hardware:

- [ ] Menu row 0 is `Overview`; remaining rows are the active detectors
- [ ] A detector that cannot fire does **not** appear (e.g. Evil-Twin vanishes when the allowlist is empty; all IDS modules vanish when `wifi_ids.enabled` is false)
- [ ] `*` marker only on armed modules; color swatch matches the alert LED color
- [ ] Overview shows all detectors with armed state, count, and age (`--` when never fired)
- [ ] Selecting a detector opens its stats: session count, last timestamp, last target
- [ ] Deauth stats page through protected SSIDs with `n`/`m`; per-SSID count is correct
- [ ] `Esc` returns to the menu from every view
- [ ] A deauth burst increments the session count
- [ ] Each IDS detector shows only its own events, never another's
- [ ] `Detailed` still works as the SSID-paged deauth view
- [ ] Adding a 6th detector touches only `g_allModules[]` — no `Display` change

## Risks

| Risk | Mitigation |
|---|---|
| `getEvents()` returns a copy each call → O(n) per frame per module | 5 modules × small vectors at 1 Hz is fine; cache per tick only if it shows |
| String churn in menu render causes flicker | Text-only, ~6 rows; measure before optimizing |
| `static String buf` in `deauthScopeName` is not reentrant | Main-loop only; constraint documented in the comment |
| IDS sub-detectors share one object → filtering could double-count | Each event carries exactly one `detector` name, so filtering partitions cleanly |
| `configManager.getConfig()` returns a reference into live config | Read-only use in `canFire()`; confirm `getConfig()` returns a stable reference, not a temporary |
| Live `config.txt` is not in the repo | Menu derives from runtime state, never a hardcoded list — this is exactly why `canFire()` exists |
