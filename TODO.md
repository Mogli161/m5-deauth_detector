# TODO

## Menu: list individual defense modules + per-detector stats + dashboard overview

The on-device menu (`Display`/`main.cpp` `VIEW_MENU`) currently has fixed entries
(Dashboard / Live Log / Detailed / Gif) that don't reflect the growing set of
detectors. As more detection modules get added (deauth, beacon-flood, evil-twin,
karma, pnl-leak, and whatever comes later), the UI needs to scale with them
instead of lumping everything into one generic view.

Planned changes:

1. **List each defense/detector module as its own menu entry** — not just a
   single generic "Dashboard" item. E.g. "Deauth", "Beacon-Flood", "Evil-Twin",
   "KARMA/MANA", "PNL-Leak" each get their own line in `VIEW_MENU`, driven by a
   data-driven list (module name + event count + LED color) rather than
   hardcoded views, so adding a new detector later doesn't require new menu
   wiring every time.

2. **Per-detector statistics view** — selecting a module from the menu should
   show its own stats screen: event count (session + since-boot), last event
   timestamp, last triggering SSID/BSSID/MAC, and the detector's refractory
   state. Right now `WifiIDSEvent`/`DeauthEvent` are only ever shown as a flat
   combined list (`VIEW_LIVE_LOG`/`VIEW_DETAILED`); there's no per-detector
   breakdown.

3. **Dashboard: short overview of everything** — a single top-level screen
   that shows, at a glance, all detectors' current status in one place (e.g.
   a compact grid/list: detector name, armed/monitoring state, event count,
   last-seen time) instead of having to open each module individually to see
   whether anything fired.

Needs: extending `Display`/`DisplayView` with a data-driven module list (likely
a small `struct DetectorModuleInfo { name, event_count, last_event_ts, led_color,
getEvents() }` abstraction shared by `DeauthDetector` and `WifiIDSDetector` so
the display code doesn't need detector-specific branches), plus new display
functions for per-module stats and the overview dashboard.
