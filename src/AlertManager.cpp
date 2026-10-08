#include "AlertManager.h"
#include "Logger.h"
#include <M5Cardputer.h>
#include <FastLED.h>

// Pin definitions for M5 Cardputer
#define LED_PIN 21
#define NUM_LEDS 1
#define LED_BRIGHTNESS 64

// FastLED setup for SK6812
CRGB leds[NUM_LEDS];

AlertManager::AlertManager(HardwareConfig &config)
    : hwConfig(config), alertActive(false), alertStartTime(0),
      lastPacketTime(0), ledTimer(0), ledCountdownActive(false),
      alertColor(0xFF0000) {}

void AlertManager::begin()
{
    // Initialize LED (SK6812)
    FastLED.addLeds<SK6812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    setLED(0x000000); // Start with LED off

    M5Cardputer.Display.setBrightness(hwConfig.screen_brightness);
}

void AlertManager::triggerAlert(uint32_t color)
{
    alertActive = true;
    alertStartTime = millis();
    lastPacketTime = millis();
    alertColor = color;

    // Buzzer removed: alert is now signaled via blinking LED only, color
    // keyed to the detector that fired (red=deauth, orange=beacon-flood,
    // magenta=evil-twin, cyan=karma, white=pnl-leak).
    setBuzzer(true);

    // Start LED countdown
    ledCountdownActive = true;
    ledTimer = millis();

    char buf[48];
    snprintf(buf, sizeof(buf), "Alert triggered! (LED signal, color 0x%06X)", color);
    logger.debugPrintln(buf);
}

void AlertManager::update()
{
    // Handle LED alert-blink duration (replaces buzzer timing)
    if (alertActive)
    {
        unsigned long sinceAlert = millis() - alertStartTime;
        if (sinceAlert > hwConfig.buzzer_duration_ms)
        {
            setBuzzer(false);
        }
        else
        {
            // Blink the alert's color every 150ms while the window is active
            bool on = ((millis() / 150) % 2) == 0;
            setLED(on ? alertColor : 0x000000);
        }
    }

    // Handle LED countdown
    if (ledCountdownActive)
    {
        unsigned long silenceTime = millis() - lastPacketTime;

        // Check if silence gap has been reached
        if (silenceTime > 30000)
        { // 30 second default silence gap
            // Start 5-minute countdown
            unsigned long countdownTime = millis() - ledTimer;
            if (countdownTime > 300000)
            { // 5 minutes
                // Turn off LED
                setLED(0x000000);
                ledCountdownActive = false;
                alertActive = false;
                logger.debugPrintln("Alert cleared after silence period");
            }
        }
        else
        {
            // Reset countdown if packets are still coming
            ledTimer = millis();
        }
    }
}

void AlertManager::setBuzzer(bool state)
{
    // Buzzer hardware removed. Kept as the LED-signal entry point so callers
    // (and the public API) don't need to change: true = alert LED active
    // (in the current alert's color), false = clear the alert LED.
    if (state)
    {
        setLED(alertColor);
    }
    else
    {
        setLED(0x000000);
    }
}

void AlertManager::setLED(uint32_t color)
{
    // Extract RGB components from 32-bit color
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Set LED color using FastLED
    leds[0] = CRGB(r, g, b);
    FastLED.show(LED_BRIGHTNESS);

    char buf[64];
    snprintf(buf, sizeof(buf), "LED color set to: 0x%06X (R:%d G:%d B:%d)", color, r, g, b);
    logger.debugPrintln(buf);
}

// LED status indicators for system states
void AlertManager::setStatusConnecting()
{
    setLED(0xFFFF00); // Yellow
    logger.debugPrintln("LED Status: Connecting to WiFi");
}

void AlertManager::setStatusSyncing()
{
    setLED(0x0000FF); // Blue
    logger.debugPrintln("LED Status: Syncing time");
}

void AlertManager::setStatusScanning()
{
    setLED(0xFFFF00); // Yellow
    logger.debugPrintln("LED Status: Scanning WiFi channels");
}

void AlertManager::setStatusReady()
{
    setLED(0x000000); // Off
    logger.debugPrintln("LED Status: System ready");
}
