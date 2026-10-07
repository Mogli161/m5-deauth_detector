#ifndef DISPLAY_H
#define DISPLAY_H

#include <M5Cardputer.h>
#include "DeauthDetector.h"
#include "Config.h"

class AlertManager; // fwd decl, used only for LED control during intro

enum DisplayView {
    VIEW_MENU,
    VIEW_DASHBOARD,
    VIEW_LIVE_LOG,
    VIEW_DETAILED,
    VIEW_GIF,
    VIEW_CONFIG_INFO
};

class Display {
public:
    Display();
    void begin();
    void showStartup();
    void showAnimatedIntro(AlertManager* alertManager);
    void showConfigMode();
    void showMonitoring();
    void showDashboard(const std::vector<String>& ssids, DeauthDetector& detector);
    void showLiveLog(const std::vector<DeauthEvent>& events);
    void showDetailed(const std::vector<String>& ssids, DeauthDetector& detector);

    // Main menu
    void showMenu();
    void menuUp();
    void menuDown();
    int getMenuIndex() { return menuIndex; }
    int getMenuItemCount();
    DisplayView menuIndexToView(int index);

    void nextDetailedPage(int maxIndex);
    void prevDetailedPage(int maxIndex);
    DisplayView getCurrentView() { return currentView; }
    void setView(DisplayView v) { currentView = v; }
    void clearScreen();

private:
    DisplayView currentView;
    int detailedPageIndex;
    int menuIndex;
    void drawHeader(const String& title);
    void drawFooter();
    String formatTime(time_t timestamp);
    String formatDateTime(time_t timestamp);
};

#endif
