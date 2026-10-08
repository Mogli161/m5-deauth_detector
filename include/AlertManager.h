#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class AlertManager {
public:
    AlertManager(HardwareConfig& config);
    void begin();
    // color defaults to red (0xFF0000) for backward compat with plain deauth
    // alerts; WifiIDS alert types pass a distinct color so the LED itself
    // tells apart deauth vs. beacon-flood vs. evil-twin vs. KARMA vs. PNL —
    // no sound is ever used for any alert, LED color coding only.
    void triggerAlert(uint32_t color = 0xFF0000);
    void update();
    bool isAlerting() { return alertActive; }    
    void setBuzzer(bool state);
    void setLED(uint32_t color);
    void setStatusConnecting();
    void setStatusSyncing();
    void setStatusScanning();
    void setStatusReady();

private:
    HardwareConfig& hwConfig;
    bool alertActive;
    unsigned long alertStartTime;
    unsigned long lastPacketTime;
    unsigned long ledTimer;
    bool ledCountdownActive;
    uint32_t alertColor; // color of the currently-active alert blink
    
};

#endif
