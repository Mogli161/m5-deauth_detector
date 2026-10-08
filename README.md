# M5 Cardputer Deauth Detector

A portable Wi-Fi security monitor for the M5Stack Cardputer that detects and alerts on deauthentication attacks targeting your protected networks.

![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/Framework-PlatformIO-orange)
![License](https://img.shields.io/badge/License-Educational-green)

## Overview

The Deauth Detector transforms your M5Stack Cardputer into a dedicated security monitoring device that continuously scans for 802.11 deauthentication frames—a common attack vector used to disconnect clients from wireless networks.

## Key Features

- **Real-time Attack Detection** — Monitors for deauthentication packets on protected networks
- **Multi-channel Scanning** — Intelligently hops only across channels where your networks broadcast
- **Hardware Alerts** — Immediate buzzer and LED notification when attacks are detected
- **Web Configuration** — Browser-based setup interface with tabbed configuration
- **SD Card Logging** — Persistent CSV logging of all detected events
- **API Reporting** — Batch reporting to external security systems via REST API
- **Alert Integrations** — Extensible alerting via SMTP email, WhatsApp, Telegram, SMS, ntfy, MQTT and more
- **NTP Time Sync** — Accurate timestamps for all logged events

## Hardware Requirements

| Component | Specification |
|-----------|---------------|
| Device | M5Stack Cardputer (ESP32-S3 StampS3) |
| Storage | MicroSD card (FAT32 formatted) |
| Display | Built-in 240×135 ST7789 TFT LCD |
| Input | Integrated keyboard + G0 button |
| Alerts | SK6812 RGB LED + Buzzer |

## Quick Start

1. **Flash the firmware** using PlatformIO
2. **Insert a FAT32 formatted SD card**
3. **Power on** — device enters configuration mode automatically
4. **Connect to the `M5-DeauthDetector` WiFi network**
5. **Navigate to `http://192.168.4.1`** and configure your settings
6. **Save and restart** — monitoring begins automatically

## Documentation

Full documentation is available in the [docs](docs/) folder:

| Document | Description |
|----------|-------------|
| [Getting Started](docs/getting-started.md) | Installation and initial setup |
| [Configuration](docs/configuration.md) | Complete configuration reference |
| [Web Interface](docs/web-interface.md) | Web portal usage guide |
| [Operation Guide](docs/operation.md) | Using the device and display views |
| [API Integration](docs/api-integration.md) | REST API payload and integration |
| [Alert Integrations](docs/alert-integrations.md) | SMTP, WhatsApp, Telegram, SMS, MQTT and more |
| [Troubleshooting](docs/troubleshooting.md) | Common issues and solutions |

## Project Structure

```
deauth_detector/
├── docs/                 # Documentation
├── include/              # Header files
├── src/                  # Source files
├── config.txt.example    # Example configuration
├── platformio.ini        # Build configuration
└── README.md
```

## Building

```bash
# Clone the repository
git clone https://github.com/your-repo/deauth_detector.git

# Build and upload with PlatformIO
pio run --target upload

# Monitor serial output (optional)
pio device monitor
```

## Dependencies

- M5Cardputer library
- ArduinoJson
- FastLED
- ESP32 WiFi/SD (built-in)

## Author

Stewart Moss © 2026

Fork maintained by Mogli161. Changes in commit `b4d61f0` ("Replace buzzer with LED
signaling, remove web UI, add menu + embedded GIF view") and the WifiIDS/SerialConsole
commit adding beacon-flood/evil-twin/KARMA/PNL-leak detection and a USB serial CLI were
implemented with AI assistance (Claude, via a Hermes agent session) based on the
maintainer's requirements; see each commit's `Co-authored-by` trailer.

## WifiIDS detector (beacon-flood / evil-twin / KARMA / PNL-leak)

In addition to deauth/disassoc detection, this fork includes a passive WiFi-IDS
(`WifiIDSDetector`, ported from a subset of a private passive 802.11 monitor) that also
flags:

- **Beacon-flood / fake-AP storm** — many new BSSIDs appearing in a short window
- **Evil twin** — your SSID beaconed from an unexpected/un-allowlisted BSSID
- **KARMA/MANA** — a rogue AP answering probe-requests for SSIDs it never beacons
- **PNL leak** — a client broadcasting its saved-network list via directed probes

Enable it in `deauthconfig.txt` under `"wifi_ids": { "enabled": true, ... }` (see
`config.txt.example`). Each detector signals via a distinct LED color — **never a sound**,
by design: red=deauth, orange=beacon-flood, magenta=evil-twin, cyan=karma, white=pnl-leak.
Alerts are reported to the same `api.endpoint_url` with an `event_type` field.

## USB serial console

A tiny line-based text CLI is available over the same USB serial port used for
flashing/logging (115200 baud) — useful for scripted/agent access without needing
`esptool` flash-reads. Commands: `HELP`, `VERSION`, `STATUS`, `EVENTS`, `IDS`, `CLEAR`.

## License

This project is provided for educational and security research purposes only. Use responsibly and only on networks you own or have explicit permission to monitor.

