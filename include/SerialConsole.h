#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H

#include <Arduino.h>
#include "DeauthDetector.h"
#include "WifiIDSDetector.h"
#include "Config.h"

// Tiny non-blocking text CLI over the USB serial port, for scripted/agent
// access (esptool flash-reads and timing-sensitive `cat /dev/ttyACM0`
// captures both miss transient state and are slow to script against).
// Line-buffered: accumulate chars until \n or \r, then dispatch. Call
// poll() once per main loop iteration; it never blocks.
class SerialConsole {
public:
    SerialConsole();
    void begin();
    // References must outlive the console (they're the same globals main.cpp
    // already owns for the whole run).
    void attach(DeauthDetector* deauth, WifiIDSDetector* ids, AppConfig* config);
    void poll();

private:
    String lineBuf;
    DeauthDetector* deauthDetector;
    WifiIDSDetector* wifiIdsDetector;
    AppConfig* appConfig;

    void dispatch(const String& line);
    void cmdHelp();
    void cmdStatus();
    void cmdEvents();
    void cmdIds();
    void cmdClear();
    void cmdVersion();
};

extern SerialConsole serialConsole;

#endif
