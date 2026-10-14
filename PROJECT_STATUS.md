# Projekt-Status: m5-deauth_detector

**Stand:** 2026-10-14, ~01:55
**Repo:** https://github.com/Mogli161/m5-deauth_detector
**Branch:** `feat/detector-menu-stats-dashboard`
**PR:** [#2](https://github.com/Mogli161/m5-deauth_detector/pull/2) — offen, bisher nur Docs
**Issue:** [#1](https://github.com/Mogli161/m5-deauth_detector/issues/1) — offen, wird bei Fertigstellung geschlossen

## Was ist der aktive Auftrag?

Umsetzung von Issue #1: das hardcoded 4-Eintrag-Menü (Dashboard / Live Log /
Detailed / Gif) durch ein datengetriebenes Menü ersetzen.

**Der Plan ist fertig, der Code ist nicht geschrieben.** Der Plan ist die
einzige committete Arbeit — bewusst, damit du ihn reviewen kannst bevor
jemand Code tippt.

## Nächster Schritt

Plan durchlesen (`docs/plans/2026-10-14-detector-menu-stats-dashboard.md`,
8 Tasks: 0–7), dann Tasks umsetzen. Vorgeschlagene Aufteilung aus der Session:

- **Tasks 1+2** (Header + Adapter) — Mechanik, Code liegt fertig im Plan, kein Subagent nötig
- **Task 0** (Arrow-Keys + Ticker) — eigenständig, isoliert in `main.cpp` + Footer
- **Tasks 3+4** (Menü + Views) — zusammen, sie teilen `Display.h`; getrennte Subagents würden sich überschreiben
- **Task 6** (Serial-Console) — parallel, unabhängig
- **Tasks 5, 7** — Task 5 ist kein Code-Change, Task 7 ist Docs + Issue schließen

Empfohlener Agent: **Codex** (C++/Arduino-Firmware, kein Web-Framework).

## Entschiedene Designpunkte

Diese fünf Dinge wurden in der Session geklärt — nicht wieder neu verhandeln:

1. **Overview als Menüeintrag 0**, Detektoren starten bei Index 1 (`getMenuIndex() - 1`)
2. **Session-Counts only**, keine Boot-Counter — ausdrücklich verworfen
3. **`Detailed`/SSID-Paging bleibt** unverändert erhalten
4. **Nur Detektoren die wirklich feuern können bekommen einen Menüpunkt** (`canFire()`-Filter)
5. **Echte Pfeiltasten funktionieren**, `;`/`.`/`n`/`m` als Fallback; jede View zeigt unten eine Bauchbinde mit ihrer Belegung

## Erkenntnisse aus der Code-Analyse (im Plan dokumentiert, hier kurz)

- **Alle 5 Detektoren sind implementiert und verdrahtet** — keine Stubs
- **Die 4 IDS-Sub-Detektoren sind nicht einzeln schaltbar** — ein einziges Flag `wifi_ids.enabled`, keine Per-Detektor-Toggles
- **`evil_twin` ist ohne Allowlist inert** — der critical-Pfad braucht einen `cfg.allowlist`-Eintrag, sonst nur info-level Rauschen. Deshalb versteckt `canFire()` es aus dem Menü
- **`config.txt` ist gitignored** — welche Detektoren auf dem echten Gerät laufen, ist aus dem Repo nicht bestimmbar. Das Menü leitet seinen Inhalt vom Laufzeitzustand ab
- **`WifiIDSDetector` hat kein `isEnabled()`** — `enabled` ist privat ohne Getter, Task 2 fügt ihn hinzu
- **Pfeiltasten gibt es nur auf der fn-Ebene** — dort bleibt `status.word` leer (`Keyboard.cpp` PASS 2 returned früh). Beide Pfade müssen akzeptiert werden

## Bug im bestehenden Code (gefunden, noch nicht gefixt)

`handleConfigMode()` (`src/main.cpp:225`) prüft nur `keyStateHasChar(status, '`')`.
Auf der fn-Ebene ist `` ` `` Esc → `status.esc == true` bei **leerem**
`status.word`. **fn+Esc verlässt den Config-Mode also nicht.** Task 0 fixt das.

## Aufbau-Wissen

- **PlatformIO-Env:** `m5stack-stamps3` (nicht `m5cardputer`) — einziges Env in `platformio.ini`
- **Build:** `pio run -e m5stack-stamps3`
- **Bildschirm:** 240×135, `setRotation(1)`, Footer-Band bisher `fillRect(0, 125, 240, 10, BLUE)`
- **Tasten:** `` ` `` = Esc · `;` = hoch · `.` = runter · `n`/`m` = Seite (Arrow-Emulation)

## Einschränkung, die wichtig ist

**Es kann auf diesem Host nicht gebaut werden.** `pio` ist nicht installiert,
das Ziel ist ein ESP32-S3. Jeder Implementierer kann nur behaupten
„compiled clean" — verifizieren lässt sich das erst beim Flashen auf den
Cardputer. Die 2-Stufen-Reviews (Spec, dann Code) fangen Logikfehler, keine
Compile-Fehler.

## Dateien

| Datei | Inhalt |
|---|---|
| `docs/plans/2026-10-14-detector-menu-stats-dashboard.md` | Der Plan, 8 Tasks, kompletter copy-pastebarer Code |
| `src/Display.cpp:4-5` | Statisches Menü — Startpunkt für Task 3 |
| `src/Display.cpp:330-350` | `showDashboard()` — SSID-only, wird zu Overview |
| `src/main.cpp:32-36` | LED-Farben pro Detektor |
| `src/main.cpp:46-53` | `colorForIdsDetector()` — Name→Farbe |
| `src/main.cpp:67-70` | Tasten-Defines |
