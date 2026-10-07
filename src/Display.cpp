#include "Display.h"
#include "AlertManager.h"

static const char* MENU_ITEMS[] = { "Dashboard", "Live Log", "Detailed", "Gif" };
static const int MENU_ITEM_COUNT = 4;

Display::Display() : currentView(VIEW_MENU), detailedPageIndex(0), menuIndex(0) {}

int Display::getMenuItemCount() { return MENU_ITEM_COUNT; }

void Display::menuUp() {
    menuIndex--;
    if (menuIndex < 0) menuIndex = MENU_ITEM_COUNT - 1;
}

void Display::menuDown() {
    menuIndex++;
    if (menuIndex >= MENU_ITEM_COUNT) menuIndex = 0;
}

DisplayView Display::menuIndexToView(int index) {
    switch (index) {
        case 0: return VIEW_DASHBOARD;
        case 1: return VIEW_LIVE_LOG;
        case 2: return VIEW_DETAILED;
        case 3: return VIEW_GIF;
        default: return VIEW_MENU;
    }
}

void Display::showMenu() {
    clearScreen();
    drawHeader("Menu");

    int y = 30;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (i == menuIndex) {
            M5Cardputer.Display.fillRect(5, y - 2, 230, 16, 0x39C7); // highlight bar (dark gray-blue)
            M5Cardputer.Display.setTextColor(WHITE, 0x39C7);
            M5Cardputer.Display.setCursor(10, y);
            M5Cardputer.Display.print("> ");
        } else {
            M5Cardputer.Display.setTextColor(WHITE, BLACK);
            M5Cardputer.Display.setCursor(10, y);
            M5Cardputer.Display.print("  ");
        }
        M5Cardputer.Display.print(MENU_ITEMS[i]);
        M5Cardputer.Display.setTextColor(WHITE, BLACK);
        y += 18;
    }

    M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
    M5Cardputer.Display.setCursor(5, 120);
    M5Cardputer.Display.print("Up/Down select, Enter open, Esc back");
}

void Display::begin()
{
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
    clearScreen();
}

void Display::clearScreen()
{
    M5Cardputer.Display.fillScreen(BLACK);
}

void Display::drawHeader(const String &title)
{
    M5Cardputer.Display.fillRect(0, 0, 240, 20, BLUE);
    M5Cardputer.Display.setTextColor(WHITE, BLUE);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print(title);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
}

void Display::showStartup()
{
    clearScreen();
    M5Cardputer.Display.setCursor(50, 50);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.println("Deauth");
    M5Cardputer.Display.setCursor(50, 70);
    M5Cardputer.Display.println("Detector");
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(40, 100);
    M5Cardputer.Display.println("Initializing...");
    M5Cardputer.Display.setCursor(1, 110);
    M5Cardputer.Display.println("By Stewart Moss (c) 2026.");
}

void Display::showAnimatedIntro(AlertManager* alertManager)
{
#define INTRO_DURATION 4000
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

    // Shield center position
    const int shieldX = SCREEN_WIDTH / 2;
    const int shieldY = 58;
    const int shieldSize = 32;

    // Progress bar dimensions
    const int barX = 30;
    const int barY = 118;
    const int barWidth = 180;
    const int barHeight = 10;

    unsigned long introStart = millis();
    bool skipped = false;
    float sweepAngle = 0;
    int lastSoundPhase = -1;

    while ((millis() - introStart < INTRO_DURATION) && !skipped)
    {
        unsigned long elapsed = millis() - introStart;
        float progress = (float)elapsed / INTRO_DURATION;

        // Clear screen
        clearScreen();

        // === Draw WiFi Shield Icon ===
        // Shield body (rounded rectangle base)
        M5Cardputer.Display.fillRoundRect(shieldX - shieldSize / 2, shieldY - shieldSize / 2 + 5,
                                          shieldSize, shieldSize + 10, 6, BLUE);
        // Shield point (triangle at bottom)
        M5Cardputer.Display.fillTriangle(shieldX - shieldSize / 2, shieldY + shieldSize / 2 + 5,
                                         shieldX + shieldSize / 2, shieldY + shieldSize / 2 + 5,
                                         shieldX, shieldY + shieldSize / 2 + 18, BLUE);

        // WiFi arcs on shield (concentric arcs using circles)
        for (int i = 3; i >= 1; i--)
        {
            int arcRadius = 6 + i * 5;
            // Draw partial circle (upper arc) - using filled circles with black overlay
            M5Cardputer.Display.drawCircle(shieldX, shieldY + 10, arcRadius, WHITE);
        }
        // Center dot
        M5Cardputer.Display.fillCircle(shieldX, shieldY + 10, 3, WHITE);

        // === Radar Sweep Effect ===
        sweepAngle = (elapsed / 8.0f); // Rotate over time
        float radians = sweepAngle * PI / 180.0f;
        int sweepLength = 45;
        int endX = shieldX + (int)(cos(radians - PI / 2) * sweepLength);
        int endY = shieldY + (int)(sin(radians - PI / 2) * sweepLength);

        // Draw sweep line with gradient effect (multiple lines)
        for (int i = 0; i < 8; i++)
        {
            float offsetAngle = (sweepAngle - i * 3) * PI / 180.0f;
            int lineEndX = shieldX + (int)(cos(offsetAngle - PI / 2) * sweepLength);
            int lineEndY = shieldY + (int)(sin(offsetAngle - PI / 2) * sweepLength);
            uint16_t lineColor = (i < 2) ? WHITE : ((i < 4) ? CYAN : BLUE);
            M5Cardputer.Display.drawLine(shieldX, shieldY, lineEndX, lineEndY, lineColor);
        }

        // === Title Text ===
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setTextColor(WHITE, BLACK);

        // Animated title - slides in from left during first 500ms
        int titleX = (elapsed < 500) ? map(elapsed, 0, 500, -100, 35) : 35;
        M5Cardputer.Display.setCursor(titleX, 8);
        M5Cardputer.Display.print("DEAUTH");

        // Second word slides in from right
        int titleX2 = (elapsed < 700) ? map(elapsed, 200, 700, 240, 100) : 100;
        if (elapsed > 200)
        {
            M5Cardputer.Display.setCursor(titleX2, 8);
            M5Cardputer.Display.print("DETECTOR");
        }

        // === Progress Bar ===
        // Bar background
        M5Cardputer.Display.drawRoundRect(barX, barY, barWidth, barHeight, 3, WHITE);
        // Bar fill
        int fillWidth = (int)(progress * (barWidth - 4));
        if (fillWidth > 0)
        {
            // Gradient effect - blue to cyan
            uint16_t barColor = (progress < 0.5) ? BLUE : CYAN;
            M5Cardputer.Display.fillRoundRect(barX + 2, barY + 2, fillWidth, barHeight - 4, 2, barColor);
        }

        // === Status Text ===
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(CYAN, BLACK);
        M5Cardputer.Display.setCursor(barX, barY - 12);

        // Animated loading text
        if (elapsed < 1000)
        {
            M5Cardputer.Display.print("Initializing");
        }
        else if (elapsed < 2000)
        {
            M5Cardputer.Display.print("Loading config");
        }
        else if (elapsed < 3000)
        {
            M5Cardputer.Display.print("Starting scanner");
        }
        else
        {
            M5Cardputer.Display.print("Ready!");
        }

        // Loading dots animation
        int dots = (elapsed / 300) % 4;
        for (int i = 0; i < dots; i++)
        {
            M5Cardputer.Display.print(".");
        }

        // === Skip Text ===
        M5Cardputer.Display.setTextColor(0x7BEF, BLACK); // Gray color
        M5Cardputer.Display.setCursor(60, SCREEN_HEIGHT - 8);
        M5Cardputer.Display.print("Press any key to skip");

        // === Author Credit (appears after 1 second) ===
        if (elapsed > 1000)
        {
            M5Cardputer.Display.setTextColor(0x7BEF, BLACK);
            M5Cardputer.Display.setCursor(45, 95);
            M5Cardputer.Display.print("By Stewart Moss (c) 2026");
        }

        // === LED Signal Sequence (replaces buzzer) ===
        int soundPhase = elapsed / 100; // Change every 100ms

        if (soundPhase != lastSoundPhase)
        {
            lastSoundPhase = soundPhase;

            if (alertManager)
            {
                if (elapsed < 300)
                {
                    alertManager->setLED(0xFFFF00);
                }
                else if (elapsed < 800)
                {
                    int beepPhase = (elapsed - 300) / 100;
                    alertManager->setLED((beepPhase % 2 == 0) ? 0x00FFFF : 0x000000);
                }
                else if (elapsed < 2500)
                {
                    int pulsePhase = ((elapsed - 800) / 200) % 2;
                    alertManager->setLED(pulsePhase == 0 ? 0x0000FF : 0x00FFFF);
                }
                else if (elapsed < 3000)
                {
                    alertManager->setLED(0x000000);
                }
                else if (elapsed < 3500)
                {
                    int clickPhase = ((elapsed - 3000) / 80) % 2;
                    alertManager->setLED(clickPhase == 0 ? 0xFFFFFF : 0x000000);
                }
                else if (elapsed < 3700)
                {
                    alertManager->setLED(0x00FF00);
                }
                else if (elapsed < 3850)
                {
                    alertManager->setLED(0x008000);
                }
                else if (elapsed < 4000)
                {
                    alertManager->setLED(0x003000);
                }
            }
        }

        // === Check for Skip ===
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
        {
            skipped = true;
        }

        delay(16); // ~60fps
    }

    // Ensure LED is off after intro
    if (alertManager) alertManager->setLED(0x000000);

    // Brief pause before continuing
    delay(100);

#undef INTRO_DURATION
#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT
}

void Display::showConfigMode()
{
    clearScreen();
    drawHeader("Config Mode");

    M5Cardputer.Display.setCursor(5, 30);
    M5Cardputer.Display.println("Configuration Portal Active");
    M5Cardputer.Display.setCursor(5, 50);
    M5Cardputer.Display.println("Connect to:");
    M5Cardputer.Display.setCursor(5, 65);
    M5Cardputer.Display.println("SSID: M5-DeauthDetector");
    M5Cardputer.Display.setCursor(5, 85);
    M5Cardputer.Display.println("Navigate to:");
    M5Cardputer.Display.setCursor(5, 100);
    M5Cardputer.Display.println("http://192.168.4.1");
}

void Display::showMonitoring()
{
    clearScreen();
    drawHeader("Monitoring");

    M5Cardputer.Display.setCursor(5, 30);
    M5Cardputer.Display.println("Monitoring for deauth attacks");
    M5Cardputer.Display.setCursor(5, 50);
    M5Cardputer.Display.println("Press ENTER to cycle views");
    M5Cardputer.Display.setCursor(5, 65);
    M5Cardputer.Display.println("Hold GO for config mode");
}

void Display::showDashboard(const std::vector<String> &ssids, DeauthDetector &detector)
{
    clearScreen();
    drawHeader("Dashboard");

    int y = 30;
    for (const String &ssid : ssids)
    {
        int count = detector.getEventCountForSSID(ssid);
        M5Cardputer.Display.setCursor(5, y);
        M5Cardputer.Display.print(ssid);
        M5Cardputer.Display.setCursor(180, y);
        M5Cardputer.Display.print(count);
        y += 15;

        if (y > 120)
            break; // Screen limit
    }

    drawFooter();
}

void Display::showLiveLog(const std::vector<DeauthEvent> &events)
{
    clearScreen();
    drawHeader("Live Log");

    int y = 30;
    int count = 0;

    // Show last 5 events
    for (int i = events.size() - 1; i >= 0 && count < 5; i--)
    {
        const DeauthEvent &event = events[i];

        M5Cardputer.Display.setCursor(5, y);
        M5Cardputer.Display.print(formatTime(event.timestamp));

        M5Cardputer.Display.setCursor(5, y + 10);
        M5Cardputer.Display.print("SSID: ");
        M5Cardputer.Display.print(event.target_ssid.substring(0, 12));

        M5Cardputer.Display.setCursor(5, y + 20);
        M5Cardputer.Display.print("Ch:");
        M5Cardputer.Display.print(event.channel);
        M5Cardputer.Display.print(" RSSI:");
        M5Cardputer.Display.print(event.rssi);

        y += 35;
        count++;

        if (y > 120)
            break;
    }

    if (events.empty())
    {
        M5Cardputer.Display.setCursor(5, 60);
        M5Cardputer.Display.println("No events detected");
    }

    drawFooter();
}

void Display::showDetailed(const std::vector<String> &ssids, DeauthDetector &detector)
{
    clearScreen();

    if (ssids.empty())
    {
        drawHeader("Details");
        M5Cardputer.Display.setCursor(5, 60);
        M5Cardputer.Display.println("No SSIDs configured");
        drawFooter();
        return;
    }

    if (detailedPageIndex >= ssids.size())
    {
        detailedPageIndex = 0;
    }

    const String &ssid = ssids[detailedPageIndex];

    // Draw header with page indicator
    M5Cardputer.Display.fillRect(0, 0, 240, 20, BLUE);
    M5Cardputer.Display.setTextColor(WHITE, BLUE);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print("Details: " + ssid.substring(0, 10));

    // Show page indicator on the right side of header
    String pageIndicator = String(detailedPageIndex + 1) + "/" + String(ssids.size());
    int indicatorWidth = pageIndicator.length() * 6; // Approximate width
    M5Cardputer.Display.setCursor(240 - indicatorWidth - 5, 5);
    M5Cardputer.Display.print(pageIndicator);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);

    int count = detector.getEventCountForSSID(ssid);
    DeauthEvent lastEvent = detector.getLastEventForSSID(ssid);
    int channel = detector.getChannelForSSID(ssid);

    M5Cardputer.Display.setCursor(5, 30);
    M5Cardputer.Display.println("Total Packets:");
    M5Cardputer.Display.setCursor(5, 45);
    M5Cardputer.Display.print("  ");
    M5Cardputer.Display.println(count);

    M5Cardputer.Display.setCursor(5, 60);
    M5Cardputer.Display.println("Channel:");
    M5Cardputer.Display.setCursor(5, 75);
    M5Cardputer.Display.print("  ");
    M5Cardputer.Display.println(channel > 0 ? String(channel) : "N/A");

    M5Cardputer.Display.setCursor(5, 90);
    M5Cardputer.Display.println("Last Attacker MAC:");
    M5Cardputer.Display.setCursor(5, 105);
    M5Cardputer.Display.print("  ");
    M5Cardputer.Display.println(lastEvent.attacker_mac);

    drawFooter();
}

// nextView() removed — navigation is now handled by the main menu (see showMenu/menuUp/menuDown)

void Display::nextDetailedPage(int maxIndex)
{
    if (maxIndex <= 0)
        return;
    detailedPageIndex++;
    if (detailedPageIndex >= maxIndex)
    {
        detailedPageIndex = 0;
    }
}

void Display::prevDetailedPage(int maxIndex)
{
    if (maxIndex <= 0)
        return;
    detailedPageIndex--;
    if (detailedPageIndex < 0)
    {
        detailedPageIndex = maxIndex - 1;
    }
}

String Display::formatTime(time_t timestamp)
{
    if (timestamp == 0)
        return "N/A";

    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    return String(buffer);
}

String Display::formatDateTime(time_t timestamp)
{
    if (timestamp == 0)
        return "N/A";

    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

void Display::drawFooter()
{
    time_t now = time(nullptr);
    String dateTime = formatDateTime(now);

    // Draw footer background
    M5Cardputer.Display.fillRect(0, 125, 240, 10, BLUE);
    M5Cardputer.Display.setTextColor(WHITE, BLUE);
    M5Cardputer.Display.setCursor(5, 127);
    M5Cardputer.Display.print(dateTime);
    M5Cardputer.Display.setTextColor(WHITE, BLACK);
}
