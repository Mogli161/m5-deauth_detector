# Projekt-Status: m5-deauth_detector

**Stand:** 2026-10-24
**Repo:** https://github.com/Mogli161/m5-deauth_detector
**Branch:** `feat/detector-menu-stats-dashboard`
**PR:** [#2](https://github.com/Mogli161/m5-deauth_detector/pull/2) — Plan + 4 Firmware-Bugfixes
**Issue:** [#1](https://github.com/Mogli161/m5-deauth_detector/issues/1) — **offen**, eigentliche Menü-Umsetzung steht noch aus

## Stand 2026-10-24 — Session abgeschlossen, Projekt läuft weiter

Das Projekt ist **dauerhaft** und wird weiter bearbeitet. In dieser Session
wurden die losen Enden geschlossen, nicht das Feature:

- 4 Firmware-Bugs gefixt (SD-Log-Spam, Null-Deref auf dem Boot-Fehler-Pfad,
  Config-Reload beim Verlassen des Config-Mode, veralteter Config-Screen-Text)
- Build verifiziert: `pio run -e m5stack-stamps3` → `[SUCCESS]`, RAM 27,5 %,
  Flash 43,6 %
- Plan-Dokument um die widerlegte fn/Arrow-Annahme korrigiert

**Offen bleibt Issue #1 selbst:** das datengetriebene Menü ist weiterhin nur
geplant, nicht implementiert. Nächster Einstieg: Plan `docs/plans/…` Tasks 0–7,
2-Stufen-Review je Task, kein Test-Suite vorhanden.

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
- ~~**Pfeiltasten gibt es nur auf der fn-Ebene** — dort bleibt `status.word` leer~~ — **widerlegt**, siehe Korrektur unten: `KeysState` hat keine `up/down/left/right/esc`-Felder, `fn` ändert das Zeichen nicht

## Korrektur (2026-10-19): der "fn+Esc-Bug" existiert NICHT

Die frühere Annahme war falsch: sie lautete, auf der fn-Ebene setze die
M5-Library `status.esc`/`status.up`/`status.down` und lasse `status.word` leer.
Das gilt für die **installierte** M5Cardputer-Version nicht.

Nachgeprüft in `.pio/libdeps/m5stack-stamps3/M5Cardputer/src/utility/Keyboard/`
(v1.1.1):

- `struct KeysState` hat genau diese Felder: `tab, fn, shift, ctrl, opt, alt,
  del, enter, space, modifiers, word, hid_keys, modifier_keys`.
  **Kein `esc`, `up`, `down`, `left`, `right`.**
- `Keyboard.cpp` PASS 2 behandelt `KEY_FN` nur als Flag und **ändert das
  erzeugte Zeichen nicht** — `getKeyValue()` liefert immer unshifted/shifted.
  Es gibt keine fn-Zeichenebene.
- `fn`+`` ` `` erzeugt also ganz normal `` ` `` in `status.word`.

**Konsequenz:** `keyStateHasChar(status, '`')` war bereits korrekt, ebenso
`;`/`.`/`n`/`m`. Ein "Fix" über `status.esc`/`status.up`/… **kompiliert nicht**:

```
src/GifPlayer.cpp:82:24: error: 'struct Keyboard_Class::KeysState'
                        has no member named 'esc'
```

Task 0 des Plans darf so **nicht** umgesetzt werden (siehe Warnung im Plan).

## Gefixte Bugs (2026-10-19, kompiliert gegen M5Cardputer 1.1.1)

| Datei | Bug | Fix |
|---|---|---|
| `src/main.cpp` | **SD-Log-Spam:** `logger.logEvent()` lief für *alle* Events bei *jedem* `loop()`-Durchlauf (die Event-Liste wird erst beim Reporting-Intervall geleert) → tausende Duplikate pro Event, Flash-Verschleiß | Logging in den "neue Events"-Zweig verschoben, loggt nur `events[lastEventCount..size())` |
| `src/main.cpp` | **Null-Deref:** `wifiManager->connectSTA()` ohne Guard auf dem Boot-Fehler-Pfad (`setup()` kehrt nach `enterConfigMode()` zurück, `wifiManager` bleibt `nullptr`) | `if (wifiManager && wifiManager->connectSTA())` |
| `src/main.cpp` | Config-Mode verließ sich auf "Reboot lädt neu"; Verlassen startete Monitor-Mode mit nie `begin()`-ten Subsystemen | `configManager.loadConfig()` vor `enterMonitorMode()` |
| `src/Display.cpp` | Config-Screen warb für ein Web-Portal (SSID + `http://192.168.4.1`), das es nicht mehr gibt (kein AP, kein Webserver) | Text auf SD-Pfad `/deauthdetector/deauthconfig.txt` |

## Aufbau-Wissen

- **PlatformIO-Env:** `m5stack-stamps3` (nicht `m5cardputer`) — einziges Env in `platformio.ini`
- **Build:** `pio run -e m5stack-stamps3`
- **Bildschirm:** 240×135, `setRotation(1)`, Footer-Band bisher `fillRect(0, 125, 240, 10, BLUE)`
- **Tasten:** `` ` `` = Esc · `;` = hoch · `.` = runter · `n`/`m` = Seite (Arrow-Emulation)

## Build ist lokal möglich (Korrektur 2026-10-19)

Die frühere Aussage „kann auf diesem Host nicht gebaut werden" ist falsch.
PlatformIO läuft in einem venv:

```bash
[ -d ~/.venvs/pio ] || (mkdir -p ~/.venvs && virtualenv -q ~/.venvs/pio && ~/.venvs/pio/bin/pip install platformio)
cd /home/hermesbot/Dokumente/Projekte/m5-deauth_detector
~/.venvs/pio/bin/pio run -e m5stack-stamps3
```

Erster Lauf lädt ~1,6 GB Toolchains/Framework (~45 min auf diesem Host),
danach ~2,5 min. Letzter Stand: `[SUCCESS]`, **RAM 27,5 %, Flash 43,6 %**,
Artefakt `output/deauthdetector-0.0.1.bin` (post-build `rename_firmware.py`
greift). 

**„Compiled clean" ist damit lokal verifizierbar** — nur das Flashen braucht
den Cardputer. Die 2-Stufen-Reviews bleiben sinnvoll für Logik, aber
Compile-Fehler müssen nicht mehr geraten werden.

## Einschränkung, die wichtig bleibt

`config.txt` ist gitignored: welche Detektoren auf dem echten Gerät laufen,
ist aus dem Repo nicht bestimmbar. Das Menü muss seinen Inhalt weiterhin vom
Laufzeitzustand (`canFire()`) ableiten, nie von einer angenommenen Liste.


## Dateien

| Datei | Inhalt |
|---|---|
| `docs/plans/2026-10-14-detector-menu-stats-dashboard.md` | Der Plan, 8 Tasks, kompletter copy-pastebarer Code |
| `src/Display.cpp:4-5` | Statisches Menü — Startpunkt für Task 3 |
| `src/Display.cpp:330-350` | `showDashboard()` — SSID-only, wird zu Overview |
| `src/main.cpp:32-36` | LED-Farben pro Detektor |
| `src/main.cpp:46-53` | `colorForIdsDetector()` — Name→Farbe |
| `src/main.cpp:67-70` | Tasten-Defines |
