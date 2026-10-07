#include <M5Cardputer.h>
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "DeauthDetector.h"
#include "Display.h"
#include "GifPlayer.h"
#include "Logger.h"
#include "APIReporter.h"
#include "AlertManager.h"

// Application state
enum AppState {
    STATE_INIT,
    STATE_CONFIG_MODE,
    STATE_MONITOR_MODE
};

// Global objects
ConfigManager configManager;
WiFiManager* wifiManager = nullptr;
DeauthDetector detector;
Display display;
GifPlayer gifPlayer;
APIReporter* apiReporter = nullptr;
AlertManager* alertManager = nullptr;

AppState currentState = STATE_INIT;
unsigned long lastReportTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long goButtonPressTime = 0;
bool goButtonPressed = false;
size_t lastEventCount = 0;

// Buzzer wurde entfernt: Alerts werden jetzt ausschliesslich per LED signalisiert.
// Burst-Debounce bleibt bestehen, damit das normale periodische Router-Deauth
// (alle 2-3h) keinen Alert ausloest - erst ein echter Burst tut das.
#define BURST_THRESHOLD_COUNT 2
#define BURST_WINDOW_MS 4000
static unsigned long recentEventTimes[BURST_THRESHOLD_COUNT];
static size_t recentEventHead = 0;
static size_t recentEventFill = 0;

// Esc-Taste: Das Cardputer hat keine dedizierte Esc-Taste, daher wird
// konventionell die Backtick-Taste (`, oben links) dafuer verwendet - geht
// von jedem Menuepunkt zurueck ins Hauptmenue.
#define ESC_KEY_CHAR '`'
// Up/Down fuer die Menuenavigation (Cardputer-Arrow-Emulation).
#define MENU_UP_CHAR ';'
#define MENU_DOWN_CHAR '.'

// Define the specific pins used by the M5Cardputer for the SD card
#define SD_SPI_SCK_PIN 40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN 12

// Forward declarations
void enterConfigMode();
void enterMonitorMode();
void handleConfigMode();
void handleMonitorMode();
void updateDisplay();
bool keyStateHasChar(const Keyboard_Class::KeysState &status, char c);

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    Serial.begin(115200);
    Serial.println("\n\n=== M5 Cardputer Deauth Detector ===");
    Serial.println("Firmware v" FIRMWARE_VERSION);

    display.begin();
    display.showStartup();
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("ERROR: SD Card initialization failed!");
        M5Cardputer.Display.fillScreen(RED);
        M5Cardputer.Display.setCursor(10, 60);
        M5Cardputer.Display.println("SD CARD ERROR!");
        while (1) delay(1000);
    }
    Serial.println("SD Card initialized");

    if (!SD.exists("/deauthdetector")) {
        if (!SD.mkdir("/deauthdetector")) {
            Serial.println("Failed to create /deauthdetector directory");
            M5Cardputer.Display.println("Cant create /deauthdetector");
            while (1) delay(1000);
        }
    }

    if (!configManager.loadConfig()) {
        Serial.println("Configuration not found or invalid");
        enterConfigMode();
        return;
    }

    AppConfig& config = configManager.getConfig();

    // Alert-Manager so frueh wie moeglich initialisieren, damit die Intro
    // bereits ueber die LED (statt Buzzer) signalisieren kann.
    alertManager = new AlertManager(config.hardware);
    alertManager->begin();

    if (config.hardware.fancy_intro) {
        display.showAnimatedIntro(alertManager);
    } else {
        delay(200);
    }

    logger.setConfig(&config);
    if (!logger.begin()) {
        logger.debugPrintln("Warning: Logger initialization failed");
    }

    wifiManager = new WiFiManager(config.wifi);
    apiReporter = new APIReporter(config.api);

    display.clearScreen();
    M5Cardputer.Display.setCursor(10, 60);
    M5Cardputer.Display.println("Connecting to WiFi... "+config.wifi.sta_ssid);

    alertManager->setStatusConnecting();

    if (wifiManager->connectSTA()) {
        M5Cardputer.Display.println("Syncing time...");
        alertManager->setStatusSyncing();
        wifiManager->syncNTP(config.ntp);

        alertManager->setStatusReady();

        wifiManager->disconnect();
        M5Cardputer.Display.println("Disconnected");
    } else {
        logger.debugPrintln("Warning: Could not connect to WiFi for time sync");
        alertManager->setStatusReady();
    }

    alertManager->setStatusScanning();
    delay(1000);

    detector.begin(config.detection.protected_ssids, config.detection);
    alertManager->setStatusReady();

    enterMonitorMode();
}

void loop() {
    M5Cardputer.update();

    switch (currentState) {
        case STATE_CONFIG_MODE:
            handleConfigMode();
            break;

        case STATE_MONITOR_MODE:
            handleMonitorMode();
            break;

        default:
            break;
    }
}

// Es gibt keine Web-UI mehr (Sicherheitsflaeche entfernt). Die Konfiguration
// erfolgt ausschliesslich ueber die JSON-Datei auf der SD-Karte
// (/deauthdetector/deauthconfig.txt) - die Bildschirmanzeige reicht als
// Rueckmeldung. "Config Mode" zeigt nur noch einen Hinweis an.
void enterConfigMode() {
    logger.debugPrintln("Entering Config Mode (SD-card only, no web UI)");
    currentState = STATE_CONFIG_MODE;

    detector.stopMonitoring();

    display.showConfigMode();
}

void enterMonitorMode() {
    logger.debugPrintln("Entering Monitor Mode");
    currentState = STATE_MONITOR_MODE;

    detector.startMonitoring();

    display.setView(VIEW_MENU);
    updateDisplay();

    lastReportTime = millis();
    lastDisplayUpdate = millis();
}

void handleConfigMode() {
    // Kein Web-Portal mehr zu bedienen - Enter oder Esc bringt zurueck ins
    // Monitoring, sobald die SD-Config von Hand aktualisiert wurde (Reboot
    // laedt sie dann automatisch neu).
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        if (status.enter || keyStateHasChar(status, ESC_KEY_CHAR)) {
            enterMonitorMode();
        }
    }
}

bool keyStateHasChar(const Keyboard_Class::KeysState &status, char c) {
    for (char k : status.word) {
        if (k == c) return true;
    }
    return false;
}

void handleMonitorMode() {
    AppConfig& config = configManager.getConfig();

    detector.updateChannelHop();

    if (alertManager) {
        alertManager->update();
    }

    std::vector<DeauthEvent> events = detector.getEvents();

    if (!events.empty()) {
        if (events.size() > lastEventCount) {
            size_t newEvents = events.size() - lastEventCount;
            unsigned long now = millis();

            for (size_t i = 0; i < newEvents; i++) {
                recentEventTimes[recentEventHead] = now;
                recentEventHead = (recentEventHead + 1) % BURST_THRESHOLD_COUNT;
                if (recentEventFill < BURST_THRESHOLD_COUNT) recentEventFill++;
            }

            if (recentEventFill >= BURST_THRESHOLD_COUNT) {
                size_t oldestIdx = recentEventHead;
                unsigned long oldestTime = recentEventTimes[oldestIdx];
                if (alertManager && (now - oldestTime) <= BURST_WINDOW_MS) {
                    alertManager->triggerAlert();
                    recentEventFill = 0;
                }
            }
        }
        lastEventCount = events.size();

        for (const DeauthEvent& event : events) {
            logger.logEvent(event);
        }
    }

    unsigned long currentTime = millis();
    if (currentTime - lastReportTime >= (config.detection.reporting_interval_seconds * 1000)) {
        if (detector.hasEvents()) {
            std::vector<DeauthEvent> reportEvents = detector.getEvents();

            detector.stopMonitoring();

            if (wifiManager->connectSTA()) {
                if (apiReporter) {
                    apiReporter->sendBatch(reportEvents);
                }
                wifiManager->disconnect();
            }

            detector.clearEvents();
            lastEventCount = 0;

            detector.startMonitoring();
        }

        lastReportTime = currentTime;
    }

    if (currentTime - lastDisplayUpdate >= 1000) {
        updateDisplay();
        lastDisplayUpdate = currentTime;
    }

    // Keyboard handling: Menu navigation + Esc-back is now consistent across
    // every view (Dashboard / Live Log / Detailed / Gif).
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        DisplayView view = display.getCurrentView();

        if (view == VIEW_MENU) {
            if (status.enter) {
                DisplayView target = display.menuIndexToView(display.getMenuIndex());
                display.setView(target);
                if (target == VIEW_GIF) {
                    // Blocking playback loop; returns here once Esc is pressed inside it.
                    gifPlayer.playUntilEsc();
                    display.setView(VIEW_MENU);
                }
                updateDisplay();
            } else if (keyStateHasChar(status, MENU_UP_CHAR)) {
                display.menuUp();
                updateDisplay();
            } else if (keyStateHasChar(status, MENU_DOWN_CHAR)) {
                display.menuDown();
                updateDisplay();
            }
        } else {
            // In any other view: Esc always goes back to the main menu.
            if (keyStateHasChar(status, ESC_KEY_CHAR)) {
                display.setView(VIEW_MENU);
                updateDisplay();
            } else if (view == VIEW_DETAILED) {
                if (!status.word.empty()) {
                    if (keyStateHasChar(status, 'n')) { // Left arrow
                        display.prevDetailedPage(config.detection.protected_ssids.size());
                        updateDisplay();
                    } else if (keyStateHasChar(status, 'm')) { // Right arrow
                        display.nextDetailedPage(config.detection.protected_ssids.size());
                        updateDisplay();
                    }
                }
            }
        }
    }

    // Go-Button halten -> Config-Hinweisbildschirm (kein Web-Server mehr)
    if (digitalRead(0) == LOW) { // G0 button
        if (!goButtonPressed) {
            goButtonPressed = true;
            goButtonPressTime = millis();
        } else {
            if (millis() - goButtonPressTime >= 2000) {
                enterConfigMode();
                goButtonPressed = false;
            }
        }
    } else {
        goButtonPressed = false;
    }
}

void updateDisplay() {
    AppConfig& config = configManager.getConfig();
    std::vector<DeauthEvent> events = detector.getEvents();

    switch (display.getCurrentView()) {
        case VIEW_MENU:
            display.showMenu();
            break;

        case VIEW_DASHBOARD:
            display.showDashboard(config.detection.protected_ssids, detector);
            break;

        case VIEW_LIVE_LOG:
            display.showLiveLog(events);
            break;

        case VIEW_DETAILED:
            display.showDetailed(config.detection.protected_ssids, detector);
            break;

        case VIEW_GIF:
        case VIEW_CONFIG_INFO:
            // Handled by their own blocking loops (gifPlayer / config screen).
            break;
    }
}
