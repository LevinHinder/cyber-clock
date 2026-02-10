#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include <Preferences.h> 
#include "time.h"
#include <WiFiManager.h> 

// ==========================================
//               CONSTANTS
// ==========================================

namespace Net {
    const char* NTP_SERVER = "pool.ntp.org";
    const char* TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3"; 
}

namespace PWM {
    constexpr int CH_LED = 0;
    constexpr int CH_BUZZ = 1;
}

namespace Pins {
    constexpr int TFT_CS   = 9;
    constexpr int TFT_DC   = 8;
    constexpr int TFT_RST  = 7;
    constexpr int TFT_MOSI = 6;
    constexpr int TFT_SCLK = 5;

    constexpr int ENC_A    = 10;
    constexpr int ENC_B    = 20;
    constexpr int ENC_BTN  = 21;
    constexpr int KEY0     = 0;

    constexpr int I2C_SDA  = 1;
    constexpr int I2C_SCL  = 2;

    constexpr int LED      = 4;
    constexpr int BUZZ     = 3;
}

namespace Screen {
    constexpr int WIDTH  = 320;
    constexpr int HEIGHT = 240;
    constexpr int CX     = WIDTH / 2;
    constexpr int CY     = HEIGHT / 2;
}

namespace Colors {
    constexpr uint16_t BG         = ST77XX_BLACK;
    constexpr uint16_t GREEN      = 0x07E0;
    constexpr uint16_t ACCENT     = 0x07FF;
    constexpr uint16_t LIGHT      = 0xFD20;
    constexpr uint16_t BLUE       = 0x07FF;
    constexpr uint16_t PINK       = 0xF81F;
    constexpr uint16_t DARK       = 0x4208;
    constexpr uint16_t GRID       = 0x2104; 
    
    constexpr uint16_t TEMP       = ST77XX_RED;
    constexpr uint16_t HUM        = BLUE;
    constexpr uint16_t TVOC       = GREEN;
    constexpr uint16_t CO2        = ST77XX_YELLOW;
}

namespace Layout {
    constexpr int GRID_L     = 10;
    constexpr int GRID_R     = 310;
    constexpr int GRID_MID_X = (GRID_L + GRID_R) / 2;
    constexpr int GRID_TOP   = 75; 
    constexpr int GRID_MID   = 107;
    constexpr int GRID_BOT   = 139;
    constexpr int LBL_TOP_Y  = GRID_TOP + 4;  
    constexpr int VAL_TOP_Y  = GRID_TOP + 14; 
    constexpr int LBL_BOT_Y  = GRID_MID + 4;  
    constexpr int VAL_BOT_Y  = GRID_MID + 14; 
}

// ==========================================
//               DATA STRUCTURES
// ==========================================

enum UIMode { 
    MODE_MENU = 0, 
    MODE_CLOCK, 
    MODE_POMODORO, 
    MODE_ALARM, 
    MODE_DVD, 
    MODE_SETTINGS,      // The list of settings
    MODE_SETTINGS_EDIT, // The page to change a value
    MODE_WIFI_SETUP 
};

enum AlertLevel { ALERT_NONE = 0, ALERT_CO2, ALERT_ALARM };

enum PomodoroState { POMO_SET_WORK = 0, POMO_SET_SHORT, POMO_SET_LONG, POMO_SET_CYCLES, POMO_READY, POMO_RUNNING, POMO_PAUSED, POMO_DONE };
enum PomoPhase { PHASE_WORK, PHASE_SHORT, PHASE_LONG };

const int GRAPH_RANGES_MIN[] = { 5, 15, 30, 60, 180, 360, 720, 1440 };
const int GRAPH_RANGES_COUNT = 8;

struct AppSettings {
    int ledBrightness = 100; 
    int speakerVol = 100;

    int graphDuration = 180;
    
    // Alarm
    uint8_t alarmHour = 6;
    uint8_t alarmMinute = 0;
    bool alarmEnabled = false;

    // Pomodoro Config
    int pomoWorkMin = 25;
    int pomoShortMin = 5;
    int pomoLongMin = 15;
    int pomoCycles = 4;
};

struct EnvData {
    float temp = 0;
    float hum = 0;
    uint16_t tvoc = 0;
    uint16_t eco2 = 400;
    unsigned long lastRead = 0;
    
    static const int HIST_LEN = 320;
    uint16_t hTemp[HIST_LEN];
    uint16_t hHum[HIST_LEN];
    uint16_t hTVOC[HIST_LEN];
    uint16_t hCO2[HIST_LEN];
    int head = 0;
    unsigned long lastHistAdd = 0;
};

struct PomodoroContext {
    PomodoroState state = POMO_SET_WORK;
    PomoPhase phase = PHASE_WORK;
    int currentCycle = 1;
    unsigned long startMillis = 0;
    unsigned long pausedMillis = 0;
    unsigned long durationMs = 0;
    
    int prevVal = -1; 
    int prevBarWidth = -1; 
    String prevLabel = "";
};

struct UIContext {
    UIMode currentMode = MODE_CLOCK;
    
    // Main Menu
    int menuIndex = 0;
    static const int MENU_ITEMS = 5; 
    const char* menuLabels[MENU_ITEMS] = { "Monitor", "Pomodoro", "Alarm", "DVD", "Settings" };

    // Settings Menu
    int settingsIndex = 0;
    static const int SETTINGS_ITEMS = 5;
    const char* settingsLabels[SETTINGS_ITEMS] = { "LED Brightness", "Speaker Volume", "Graph Range", "Setup WiFi", "Reset WiFi" };
    
    // Settings Editor
    int editId = -1; // 0=LED, 1=Spk, 2=Graph
    int prevVal = -1; // To detect changes for redraw

    // Alarm UI
    bool alarmRinging = false;
    int alarmSelectedField = 0; 
    int lastAlarmDayTriggered = -1;

    // Settings UI
    int settingsSelectedRow = 0;
    // UPDATED: Now 4 rows (LED, Spk, Setup WiFi, Reset WiFi)
    static const int SETTINGS_ROWS = 5; 
    bool settingsEditMode = false;
    bool settingsInited = false;
    int prevLedBarW = -1;
    int prevSpkBarW = -1;
    
    // WiFi Setup UI
    bool wifiSetupInited = false;
    
    // Clock UI
    int prevSecond = -1;
    String prevTimeStr = "";
    
    // DVD UI
    bool dvdInited = false;
    int dvdX, dvdY, dvdVX = 2, dvdVY = 1;
    int dvdW = 80, dvdH = 30; 
    unsigned long lastDvdMs = 0;
    int dvdColorIndex = 0;
    
    // Alerting
    AlertLevel currentAlert = ALERT_NONE;
    unsigned long lastLedToggleMs = 0;
    bool ledState = false;
    unsigned long lastCo2BlinkMs = 0;
    bool co2BlinkOn = false;
};

struct InputState {
    int lastEncA = HIGH;
    int lastEncB = HIGH;
    bool lastEncBtn = HIGH;
    bool lastKey0 = HIGH;
    unsigned long lastEncBtnMs = 0;
    unsigned long lastKey0Ms = 0;
};

// ==========================================
//               GLOBALS
// ==========================================

Adafruit_ST7789 tft = Adafruit_ST7789(Pins::TFT_CS, Pins::TFT_DC, Pins::TFT_RST);
GFXcanvas16 graphCanvas(320, 90);
Adafruit_AHTX0 aht;
ScioSense_ENS160 ens160(0x53);
Preferences prefs; 
WiFiManager wm; 

AppSettings settings;
EnvData env;
PomodoroContext pomo;
UIContext ui;
InputState input;

uint16_t dvdPalette[] = { ST77XX_WHITE, Colors::ACCENT, Colors::LIGHT, Colors::GREEN, Colors::PINK, ST77XX_YELLOW };

// ==========================================
//           HARDWARE & SYSTEM HELPERS
// ==========================================

void setLedState(bool on) {
    if (on) {
        int duty = map(settings.ledBrightness, 0, 100, 0, 255);
        ledcWrite(PWM::CH_LED, duty); // Write to Channel 0
    } else {
        ledcWrite(PWM::CH_LED, 0);
    }
}

void playSystemTone(unsigned int frequency, unsigned long durationMs = 0) {
    if (settings.speakerVol == 0) {
        ledcWrite(PWM::CH_BUZZ, 0);
        return;
    }
    
    // Set up the channel with the new frequency
    ledcSetup(PWM::CH_BUZZ, frequency, 8);
    // Ensure pin is attached
    ledcAttachPin(Pins::BUZZ, PWM::CH_BUZZ);
    
    int duty = map(settings.speakerVol, 0, 100, 0, 128);
    ledcWrite(PWM::CH_BUZZ, duty);
    
    if (durationMs > 0) {
        delay(durationMs);
        stopSystemTone();
    }
}

void stopSystemTone() {
    ledcWrite(PWM::CH_BUZZ, 0);
}

void loadSettings() {
    prefs.begin("cyber", true); 
    settings.ledBrightness = prefs.getInt("led_b", 100);
    settings.speakerVol    = prefs.getInt("spk_v", 100);

    settings.graphDuration = prefs.getInt("gr_dur", 5);
    
    settings.alarmHour     = prefs.getInt("alm_h", 7);
    settings.alarmMinute   = prefs.getInt("alm_m", 0);
    settings.alarmEnabled  = prefs.getBool("alm_e", false);
    
    settings.pomoWorkMin   = prefs.getInt("p_work", 25);
    settings.pomoShortMin  = prefs.getInt("p_short", 5);
    settings.pomoLongMin   = prefs.getInt("p_long", 15);
    settings.pomoCycles    = prefs.getInt("p_cycl", 4);
    prefs.end();
}

void saveSettings() {
    prefs.begin("cyber", false); 
    prefs.putInt("led_b", settings.ledBrightness);
    prefs.putInt("spk_v", settings.speakerVol);

    prefs.putInt("gr_dur", settings.graphDuration);
    
    prefs.putInt("alm_h", settings.alarmHour);
    prefs.putInt("alm_m", settings.alarmMinute);
    prefs.putBool("alm_e", settings.alarmEnabled);
    
    prefs.putInt("p_work", settings.pomoWorkMin);
    prefs.putInt("p_short", settings.pomoShortMin);
    prefs.putInt("p_long", settings.pomoLongMin);
    prefs.putInt("p_cycl", settings.pomoCycles);
    prefs.end();
}

// Added 'int lastIndex' to parameters
void drawGenericList(const char* items[], int count, int selectedIndex, int lastIndex, bool fullRedraw) {
    if (fullRedraw) {
        tft.fillScreen(Colors::BG);
        drawAlarmIcon();
    }
    
    for (int i = 0; i < count; i++) {
        int rowH = 35;
        int startY = 50; 
        int rowCenterY = startY + i * rowH; 
        
        int boxY = rowCenterY - 14; 
        int boxH = 28; 
        int boxW = 300; 
        int boxX = 10;
        int textY = rowCenterY - 7; 
        int textX = 24;

        // FIXED LOGIC: 
        // We redraw if:
        // 1. It's a full redraw
        // 2. This is the new selected item (to highlight it)
        // 3. This is the OLD selected item (to un-highlight it)
        if (fullRedraw || i == selectedIndex || i == lastIndex) {
             bool selected = (i == selectedIndex);
             if (selected) {
                 tft.fillRect(boxX, boxY, boxW, boxH, Colors::ACCENT);
                 tft.setTextColor(Colors::BG);
             } else {
                 tft.fillRect(boxX, boxY, boxW, boxH, Colors::BG);
                 tft.setTextColor(ST77XX_WHITE);
             }
             tft.setTextSize(2); 
             tft.setCursor(textX, textY); 
             tft.print(items[i]);
        }
    }
}


// --- POMODORO HELPERS ---
void updatePomodoroValue(int value);
void drawPomodoroScreen(bool forceStatic);

int readEncoderStep() {
    int encA = digitalRead(Pins::ENC_A);
    int encB = digitalRead(Pins::ENC_B);
    int step = 0;
    if (encA != input.lastEncA) {
        if (encA == LOW) {
            if (encB == HIGH) step = +1;
            else step = -1;
        }
    }
    input.lastEncA = encA;
    input.lastEncB = encB;
    return step;
}

// Update the signature to accept 'lastMs' by reference
bool checkButtonPressed(uint8_t pin, bool &lastState, unsigned long &lastMs) {
    bool cur = digitalRead(pin);
    bool pressed = false;
    unsigned long now = millis();
    
    // Use the specific 'lastMs' timer passed in, not a global one
    if (cur == LOW && lastState == HIGH) {
        if (now - lastMs > 150) { // Debounce check
            pressed = true;
            lastMs = now; // Update the specific timer
        }
    }
    
    lastState = cur;
    return pressed;
}

String getTimeStr(char type) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "--";
    char buf[8];
    if (type == 'H') strftime(buf, sizeof(buf), "%H", &timeinfo);
    else if (type == 'M') strftime(buf, sizeof(buf), "%M", &timeinfo);
    else if (type == 'S') strftime(buf, sizeof(buf), "%S", &timeinfo);
    return String(buf);
}

void syncTime() {
    configTime(0, 0, Net::NTP_SERVER);
    setenv("TZ", Net::TIME_ZONE, 1);
    tzset();
}

// ==========================================
//               UI PRIMITIVES
// ==========================================

void printCenteredText(const String &txt, int x0, int x1, int y, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t bx, by;
    uint16_t w, h;
    tft.setTextSize(size);
    tft.getTextBounds(txt, 0, 0, &bx, &by, &w, &h);
    int x = x0 + ((x1 - x0) - (int)w) / 2;
    tft.setTextColor(color, bg);
    tft.setCursor(x, y);
    tft.print(txt);
}

void drawAlarmIcon() {
    int x = Screen::WIDTH - 24; 
    tft.fillRect(x - 10, 0, 24, 24, Colors::BG);
    if (!settings.alarmEnabled) return;
    uint16_t c = Colors::LIGHT; 
    tft.drawRoundRect(x - 9, 4, 18, 14, 4, c);
    tft.drawFastHLine(x - 7, 16, 14, c);
    tft.fillCircle(x, 19, 2, c);
}

void drawBarItem(int x, int y, int w, int h, int value, int &prevW) {
    tft.drawRect(x - 1, y - 1, w + 2, h + 2, ST77XX_WHITE);
    if (prevW == -1) {
        tft.fillRect(x, y, w, h, Colors::DARK);
        prevW = 0;
    }
    int targetW = (int)((float)w * (value / 100.0f));
    if (targetW != prevW) {
        if (targetW > prevW) {
             tft.fillRect(x + prevW, y, targetW - prevW, h, Colors::GREEN);
        } else {
             tft.fillRect(x + targetW, y, prevW - targetW, h, Colors::DARK);
        }
        prevW = targetW;
    }
}

// ==========================================
//               LOGIC: SENSORS & HISTORY
// ==========================================

void drawHistoryGraph() {
    graphCanvas.fillScreen(Colors::BG);
    int h = 90;
    graphCanvas.drawFastHLine(0, h/4,   320, Colors::GRID);
    graphCanvas.drawFastHLine(0, h/2,   320, Colors::GRID);
    graphCanvas.drawFastHLine(0, 3*h/4, 320, Colors::GRID);

    int pX = -1;
    int pY_T = -1, pY_H = -1, pY_V = -1, pY_C = -1;

    for (int i = 0; i < EnvData::HIST_LEN; i++) {
        int idx = (env.head + i) % EnvData::HIST_LEN;
        
        int valT = env.hTemp[idx] / 10; 
        int yT = map(constrain(valT, 0, 50), 0, 50, 88, 2);
        int valH = env.hHum[idx];
        int yH = map(constrain(valH, 0, 100), 0, 100, 88, 2);
        int valV = env.hTVOC[idx];
        int yV = map(constrain(valV, 0, 750), 0, 600, 88, 2);
        int valC = env.hCO2[idx];
        int yC = map(constrain(valC, 400, 1200), 400, 1200, 88, 2);
        int x = i;

        if (i > 0) {
            graphCanvas.drawLine(pX, pY_T, x, yT, Colors::TEMP); 
            graphCanvas.drawLine(pX, pY_H, x, yH, Colors::HUM);  
            graphCanvas.drawLine(pX, pY_V, x, yV, Colors::TVOC); 
            graphCanvas.drawLine(pX, pY_C, x, yC, Colors::CO2); 
        }
        pX = x; pY_T = yT; pY_H = yH; pY_V = yV; pY_C = yC;
    }
    graphCanvas.drawRect(0, 0, 320, 90, Colors::GRID);
    tft.drawRGBBitmap(0, 145, graphCanvas.getBuffer(), 320, 90);
}

void recordHistory(float t, float h, uint16_t tvoc, uint16_t eco2) {
    env.hTemp[env.head] = (uint16_t)(t * 10); 
    env.hHum[env.head]  = (uint16_t)h;
    env.hTVOC[env.head] = tvoc;
    env.hCO2[env.head]  = eco2;
    env.head = (env.head + 1) % EnvData::HIST_LEN;
    if (ui.currentMode == MODE_CLOCK) drawHistoryGraph();
}

void updateEnvSensors(bool force = false) {
    unsigned long now = millis();
    if (force || (now - env.lastRead) >= 1000) { 
        env.lastRead = now;
        sensors_event_t hum, temp;
        if (aht.getEvent(&hum, &temp)) {
            env.temp = temp.temperature;
            env.hum  = hum.relative_humidity;
        }
        ens160.set_envdata(env.temp, env.hum);
        ens160.measure();
        uint16_t newTVOC = ens160.getTVOC();
        uint16_t newCO2  = ens160.geteCO2();
        if (newTVOC != 0xFFFF) env.tvoc = newTVOC;
        if (newCO2  != 0xFFFF) env.eco2 = newCO2;
    }

    // === NEW LOGIC START ===
    // Calculate interval: (DurationMinutes * 60000ms) / 320 pixels
    unsigned long interval = (settings.graphDuration * 60000UL) / EnvData::HIST_LEN;
    
    // Safety clamp: minimum 1 second interval
    if (interval < 1000) interval = 1000;

    if (now - env.lastHistAdd >= interval) { 
        env.lastHistAdd = now;
        recordHistory(env.temp, env.hum, env.tvoc, env.eco2);
    }
    // === NEW LOGIC END ===
}

// ==========================================
//               LOGIC: SCREENS
// ==========================================

// --- CLOCK SCREEN ---

void initClockStaticUI() {
    tft.fillScreen(Colors::BG);
    drawAlarmIcon();
    tft.drawFastHLine(Layout::GRID_L, Layout::GRID_TOP, Layout::GRID_R - Layout::GRID_L, ST77XX_WHITE);
    tft.drawFastHLine(Layout::GRID_L, Layout::GRID_MID, Layout::GRID_R - Layout::GRID_L, ST77XX_WHITE);
    tft.drawFastHLine(Layout::GRID_L, Layout::GRID_BOT, Layout::GRID_R - Layout::GRID_L, ST77XX_WHITE);
    tft.drawFastVLine(Layout::GRID_MID_X, Layout::GRID_TOP, Layout::GRID_BOT - Layout::GRID_TOP, ST77XX_WHITE);

    printCenteredText("HUMI", Layout::GRID_L,     Layout::GRID_MID_X, Layout::LBL_TOP_Y, ST77XX_WHITE, Colors::BG, 1);
    printCenteredText("TEMP", Layout::GRID_MID_X, Layout::GRID_R,     Layout::LBL_TOP_Y, ST77XX_WHITE, Colors::BG, 1);
    printCenteredText("TVOC", Layout::GRID_L,     Layout::GRID_MID_X, Layout::LBL_BOT_Y, ST77XX_WHITE, Colors::BG, 1);
    printCenteredText("CO2",  Layout::GRID_MID_X, Layout::GRID_R,     Layout::LBL_BOT_Y, ST77XX_WHITE, Colors::BG, 1);
    drawHistoryGraph();
}

void drawClockTime(String hourStr, String minStr, String secStr) {
    String cur = hourStr + ":" + minStr + ":" + secStr;
    if (cur == ui.prevTimeStr) return;
    ui.prevTimeStr = cur;

    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(6); 
    tft.getTextBounds("88:88:88", 0, 0, &x1, &y1, &w, &h);
    int x = (Screen::WIDTH - w) / 2;
    int y = 10; 
    tft.setTextColor(ST77XX_WHITE, Colors::BG);
    tft.setCursor(x, y);
    tft.print(cur);
}

void drawEnvDynamic() {
    int clearH = 18; 
    int w_left  = (Layout::GRID_MID_X - Layout::GRID_L) - 2;
    int w_right = (Layout::GRID_R - Layout::GRID_MID_X) - 2;

    tft.fillRect(Layout::GRID_L + 1, Layout::VAL_TOP_Y, w_left, clearH, Colors::BG);
    char humBuf[8]; sprintf(humBuf, "%2.0f%%", env.hum);
    printCenteredText(String(humBuf), Layout::GRID_L, Layout::GRID_MID_X, Layout::VAL_TOP_Y, Colors::HUM, Colors::BG, 2);

    tft.fillRect(Layout::GRID_MID_X + 1, Layout::VAL_TOP_Y, w_right, clearH, Colors::BG);
    char tempBuf[10]; sprintf(tempBuf, "%2.1fC", env.temp);
    printCenteredText(String(tempBuf), Layout::GRID_MID_X, Layout::GRID_R, Layout::VAL_TOP_Y, Colors::TEMP, Colors::BG, 2);

    tft.fillRect(Layout::GRID_L + 1, Layout::VAL_BOT_Y, w_left, clearH, Colors::BG);
    char tvocBuf[16]; sprintf(tvocBuf, "%d", env.tvoc);
    printCenteredText(String(tvocBuf), Layout::GRID_L, Layout::GRID_MID_X, Layout::VAL_BOT_Y, Colors::TVOC, Colors::BG, 2);

    tft.fillRect(Layout::GRID_MID_X + 1, Layout::VAL_BOT_Y, w_right, clearH, Colors::BG);
    char co2Buf[12]; sprintf(co2Buf, "%d", env.eco2);
    printCenteredText(String(co2Buf), Layout::GRID_MID_X, Layout::GRID_R, Layout::VAL_BOT_Y, Colors::CO2, Colors::BG, 2);
}

void runClock(bool k0Pressed) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        int sec = timeinfo.tm_sec;
        if (sec != ui.prevSecond) {
            ui.prevSecond = sec;
            drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
            if (sec % 5 == 0) {
                updateEnvSensors(true);
                drawEnvDynamic();
            } else {
                updateEnvSensors(false);
            }
        }
    }
    if (k0Pressed) {
        ui.currentMode = MODE_MENU;
    }
}

// --- MENU SCREEN ---

void drawMenu(bool fullRedraw) {
    if (fullRedraw) {
        tft.fillScreen(Colors::BG);
        drawAlarmIcon();
        // Redraw all items
        for (int i = 0; i < ui.MENU_ITEMS; i++) {
             int rowCenterY = 60 + i * 35; 
             int boxY = rowCenterY - 14; int boxH = 28; int boxW = 300; int boxX = 10;
             int textY = rowCenterY - 7; int textX = 24;
             bool selected = (i == ui.menuIndex);
             if (selected) {
                 tft.fillRect(boxX, boxY, boxW, boxH, Colors::ACCENT);
                 tft.setTextColor(Colors::BG);
             } else {
                 tft.fillRect(boxX, boxY, boxW, boxH, Colors::BG);
                 tft.setTextColor(ST77XX_WHITE);
             }
             tft.setTextSize(2); tft.setCursor(textX, textY); tft.print(ui.menuLabels[i]);
        }
    }
}

void runMenu(int encStep, bool encPressed, bool k0Pressed) {
    static int lastMenuIndex = -1;
    
    // Draw only if index changed
    if (ui.menuIndex != lastMenuIndex) {
        // Pass lastMenuIndex to fix the scrolling highlight bug too
        drawGenericList(ui.menuLabels, ui.MENU_ITEMS, ui.menuIndex, lastMenuIndex, lastMenuIndex == -1);
        lastMenuIndex = ui.menuIndex;
    }

    if (encStep != 0) {
        lastMenuIndex = ui.menuIndex; // Save old index
        ui.menuIndex += encStep;
        if (ui.menuIndex < 0) ui.menuIndex = ui.MENU_ITEMS - 1;
        if (ui.menuIndex >= ui.MENU_ITEMS) ui.menuIndex = 0;
        
        // Use optimized draw
        drawGenericList(ui.menuLabels, ui.MENU_ITEMS, ui.menuIndex, lastMenuIndex, false);
    }
    
    if (encPressed) {
        lastMenuIndex = -1; // Reset so it redraws when we come back
        if (ui.menuIndex == 0) { // Monitor
             ui.currentMode = MODE_CLOCK;
             initClockStaticUI();
             ui.prevTimeStr = "";
             updateEnvSensors(true);
             drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
             drawEnvDynamic();
        } else if (ui.menuIndex == 1) { // Pomodoro
             ui.currentMode = MODE_POMODORO;
             pomo.state = POMO_SET_WORK;
             pomo.prevVal = -1;
             tft.fillScreen(Colors::BG);
             printCenteredText("Set Work", 0, Screen::WIDTH, 40, Colors::LIGHT, Colors::BG, 2);
             updatePomodoroValue(settings.pomoWorkMin);
        } else if (ui.menuIndex == 2) { // Alarm
             ui.currentMode = MODE_ALARM;
             ui.alarmSelectedField = 0;
             tft.fillScreen(Colors::BG);
             drawAlarmIcon();
        } else if (ui.menuIndex == 3) { // DVD
             ui.currentMode = MODE_DVD;
             ui.dvdInited = false;
        } else if (ui.menuIndex == 4) { // Settings
             ui.currentMode = MODE_SETTINGS;
             ui.settingsIndex = 0;
        }
    }
    
    if (k0Pressed) {
        lastMenuIndex = -1;
        ui.currentMode = MODE_CLOCK;
        initClockStaticUI();
    }
}

// --- POMODORO SCREEN ---

void drawPomodoroBar(float progress) {
    int barX = 20; int barY = 225; int barW = 280; int barH = 10;

    if (pomo.prevBarWidth == -1) {
        tft.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, ST77XX_WHITE);
        tft.fillRect(barX, barY, barW, barH, Colors::DARK);
        pomo.prevBarWidth = 0;
    }

    int targetW = (int)(barW * progress);
    if (targetW != pomo.prevBarWidth) {
        if (targetW > pomo.prevBarWidth) {
            tft.fillRect(barX + pomo.prevBarWidth, barY, targetW - pomo.prevBarWidth, barH, Colors::GREEN);
        } else {
            tft.fillRect(barX + targetW, barY, pomo.prevBarWidth - targetW, barH, Colors::DARK);
        }
        pomo.prevBarWidth = targetW;
    }
}

void drawPomodoroScreen(bool forceStatic) {
    if (forceStatic) {
        tft.fillScreen(Colors::BG);
        drawAlarmIcon();
        pomo.prevBarWidth = -1; 
        pomo.prevLabel = "";
        ui.prevTimeStr = ""; 
        
        char cycleBuf[20]; sprintf(cycleBuf, "Cycle: %d/%d", pomo.currentCycle, settings.pomoCycles);
        printCenteredText(cycleBuf, 0, Screen::WIDTH, 190, ST77XX_WHITE, Colors::BG, 2);
    }

    String labelStr = "";
    uint16_t labelColor = Colors::LIGHT;

    if (pomo.state == POMO_PAUSED) {
        labelStr = "PAUSED"; labelColor = ST77XX_YELLOW;
    } else {
        if (pomo.phase == PHASE_WORK) {
             labelStr = "GET TO WORK!";
             labelColor = Colors::LIGHT;
        } else if (pomo.phase == PHASE_SHORT) {
             labelStr = "SHORT BREAK";
             labelColor = Colors::GREEN;
        } else {
             labelStr = "LONG BREAK";
             labelColor = Colors::BLUE;
        }
    }

    if (labelStr != pomo.prevLabel) {
        tft.fillRect(0, 30, Screen::WIDTH, 20, Colors::BG); 
        printCenteredText(labelStr, 0, Screen::WIDTH, 30, labelColor, Colors::BG, 2);
        pomo.prevLabel = labelStr;
    }

    unsigned long elapsed = 0;
    if (pomo.state == POMO_RUNNING) elapsed = millis() - pomo.startMillis;
    else if (pomo.state == POMO_PAUSED) elapsed = pomo.pausedMillis - pomo.startMillis;
    if (elapsed > pomo.durationMs) elapsed = pomo.durationMs;

    float progress = (pomo.durationMs > 0) ? (float)elapsed / pomo.durationMs : 0.0f;
    drawPomodoroBar(progress);

    unsigned long remain = pomo.durationMs - elapsed;
    uint16_t rmMin = remain / 60000UL;
    uint8_t  rmSec = (remain / 1000UL) % 60;
    char buf[8]; sprintf(buf, "%02d:%02d", rmMin, rmSec);
    String timeStr = String(buf);
    
    static uint16_t prevTimeColor = 0;
    uint16_t timeColor = (pomo.state == POMO_PAUSED) ? Colors::LIGHT : ST77XX_WHITE;

    if (timeStr != ui.prevTimeStr || timeColor != prevTimeColor) {
         tft.setTextSize(6); tft.setTextColor(timeColor, Colors::BG); 
         int16_t x1, y1; uint16_t w, h; tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
         tft.setCursor((Screen::WIDTH - w)/2, 90); tft.print(timeStr);
         ui.prevTimeStr = timeStr;
         prevTimeColor = timeColor;
    }
}

void updatePomodoroValue(int value) {
    if (value == pomo.prevVal) return;
    pomo.prevVal = value;
    int numY = 100; int numW = 100;
    int startX = (Screen::WIDTH - numW) / 2;
    tft.fillRect(startX, numY, numW, 50, Colors::BG);
    tft.setTextSize(6); tft.setTextColor(ST77XX_WHITE, Colors::BG);
    String valStr = String(value);
    int16_t x1, y1; uint16_t w, h; tft.getTextBounds(valStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((Screen::WIDTH - w)/2, numY); tft.print(valStr);
}

void runPomodoro(int encStep, bool encPressed, bool k0Pressed) {
    if (pomo.state < POMO_READY) {
        if (encStep != 0) {
            if (pomo.state == POMO_SET_WORK) {
                settings.pomoWorkMin = constrain(settings.pomoWorkMin + encStep, 1, 90);
                updatePomodoroValue(settings.pomoWorkMin);
            } else if (pomo.state == POMO_SET_SHORT) {
                settings.pomoShortMin = constrain(settings.pomoShortMin + encStep, 1, 30);
                updatePomodoroValue(settings.pomoShortMin);
            } else if (pomo.state == POMO_SET_LONG) {
                settings.pomoLongMin = constrain(settings.pomoLongMin + encStep, 1, 60);
                updatePomodoroValue(settings.pomoLongMin);
            } else if (pomo.state == POMO_SET_CYCLES) {
                settings.pomoCycles = constrain(settings.pomoCycles + encStep, 1, 10);
                updatePomodoroValue(settings.pomoCycles);
            }
        }
        if (encPressed) {
            pomo.prevVal = -1;
            tft.fillScreen(Colors::BG);
            tft.setTextSize(2); tft.setTextColor(Colors::LIGHT, Colors::BG);
            
            if (pomo.state == POMO_SET_WORK) {
                pomo.state = POMO_SET_SHORT;
                printCenteredText("Short Break", 0, Screen::WIDTH, 40, Colors::LIGHT, Colors::BG, 2);
                updatePomodoroValue(settings.pomoShortMin);
            } else if (pomo.state == POMO_SET_SHORT) {
                pomo.state = POMO_SET_LONG;
                printCenteredText("Long Break", 0, Screen::WIDTH, 40, Colors::LIGHT, Colors::BG, 2);
                updatePomodoroValue(settings.pomoLongMin);
            } else if (pomo.state == POMO_SET_LONG) {
                pomo.state = POMO_SET_CYCLES;
                printCenteredText("Set Cycles", 0, Screen::WIDTH, 40, Colors::LIGHT, Colors::BG, 2);
                updatePomodoroValue(settings.pomoCycles);
            } else if (pomo.state == POMO_SET_CYCLES) {
                pomo.state = POMO_RUNNING;
                pomo.phase = PHASE_WORK;
                pomo.currentCycle = 1;
                pomo.durationMs = settings.pomoWorkMin * 60UL * 1000UL;
                pomo.startMillis = millis();
                drawPomodoroScreen(true);
            }
        }
    } else if (pomo.state == POMO_RUNNING || pomo.state == POMO_PAUSED) {
        if (encPressed) {
            if (pomo.state == POMO_RUNNING) {
                pomo.state = POMO_PAUSED;
                pomo.pausedMillis = millis();
            } else {
                pomo.startMillis += (millis() - pomo.pausedMillis);
                pomo.state = POMO_RUNNING;
            }
        }
        if (pomo.state == POMO_RUNNING) {
            unsigned long elapsed = millis() - pomo.startMillis;
            
            // Time is up!
            if (elapsed >= pomo.durationMs) {
                // 1. Play Alarm
                setLedState(true);
                playSystemTone(2000, 1500);
                setLedState(false);

                // 2. Decide Next Phase
                if (pomo.phase == PHASE_WORK) {
                    // LOGIC FIX: Use strictly LESS THAN (<) 
                    // If Cycle 1 < 4 -> Short Break
                    // If Cycle 4 < 4 -> False -> Long Break (Correct)
                    if (pomo.currentCycle < settings.pomoCycles) {
                        pomo.phase = PHASE_SHORT;
                        pomo.durationMs = settings.pomoShortMin * 60UL * 1000UL;
                    } else {
                        // This is the last cycle, so do the Long Break immediately
                        pomo.phase = PHASE_LONG;
                        pomo.durationMs = settings.pomoLongMin * 60UL * 1000UL;
                    }
                } else if (pomo.phase == PHASE_SHORT) {
                    // End of Short Break -> Start Next Work Cycle
                    pomo.phase = PHASE_WORK;
                    pomo.currentCycle++;
                    pomo.durationMs = settings.pomoWorkMin * 60UL * 1000UL;
                } else if (pomo.phase == PHASE_LONG) {
                    pomo.phase = PHASE_WORK;       // Go back to Work
                    pomo.currentCycle = 1;         // Reset cycle counter to 1
                    pomo.durationMs = settings.pomoWorkMin * 60UL * 1000UL; // Set duration to Work time
                }
                
                // 3. Restart Timer for next phase (if not done)
                if (pomo.state != POMO_DONE) {
                    pomo.startMillis = millis();
                    drawPomodoroScreen(true);
                }
            } else {
                drawPomodoroScreen(false);
            }
        } else {
            drawPomodoroScreen(false);
        }
    } else if (pomo.state == POMO_DONE) {
        if (encPressed) {
            pomo.state = POMO_SET_WORK;
            pomo.prevVal = -1;
            tft.fillScreen(Colors::BG);
            printCenteredText("Set Work", 0, Screen::WIDTH, 40, Colors::LIGHT, Colors::BG, 2);
            updatePomodoroValue(settings.pomoWorkMin);
        }
    }

    if (k0Pressed) {
        saveSettings(); 
        ui.currentMode = MODE_MENU;
    }
}

// --- ALARM SCREEN ---

void drawAlarmRingingScreen() {
    tft.fillScreen(ST77XX_RED);
    tft.setTextSize(4);
    tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
    tft.setCursor(60, 100);
    tft.print("ALARM!");
}

void drawAlarmScreen(bool full) {
    // 1. Static Elements (Draw only once)
    if (full) {
        tft.fillScreen(Colors::BG);
        drawAlarmIcon();
        
        tft.setTextSize(3);
        tft.setTextColor(ST77XX_WHITE, Colors::BG);
        tft.setCursor(50, 165);
        tft.print("Status:");
    }

    // 2. Dynamic Elements (Time)
    char hBuf[3], mBuf[3];
    sprintf(hBuf, "%02d", settings.alarmHour);
    sprintf(mBuf, "%02d", settings.alarmMinute);

    tft.setTextSize(6);
    
    // Calculate centering (do this math, but don't clear screen based on it)
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds("88:88", 0, 0, &x1, &y1, &w, &h);
    int x = (Screen::WIDTH - w) / 2;
    int y = 60;

    tft.setCursor(x, y);

    // Draw Hour (overwrite background to prevent flicker)
    tft.setTextColor(ui.alarmSelectedField == 0 ? Colors::LIGHT : ST77XX_WHITE, Colors::BG);
    tft.print(hBuf);

    // Draw Colon
    tft.setTextColor(ST77XX_WHITE, Colors::BG);
    tft.print(":");

    // Draw Minute
    tft.setTextColor(ui.alarmSelectedField == 1 ? Colors::LIGHT : ST77XX_WHITE, Colors::BG);
    tft.print(mBuf);

    // 3. Dynamic Elements (Status ON/OFF)
    // REMOVED: tft.fillRect(...) <--- This was causing the flicker!
    
    tft.setTextSize(3);
    tft.setCursor(180, 165);
    
    uint16_t enColor = ui.alarmSelectedField == 2 ? Colors::LIGHT : (settings.alarmEnabled ? Colors::GREEN : ST77XX_RED);
    
    // Passing Colors::BG as the second argument clears the previous text automatically
    tft.setTextColor(enColor, Colors::BG);
    
    // We add a space to "ON " so it has same length as "OFF" (3 chars)
    // This ensures "OFF" is completely overwritten when switching to "ON"
    tft.print(settings.alarmEnabled ? "ON " : "OFF");
}

void runAlarm(int encStep, bool encPressed, bool k0Pressed) {
    if (ui.alarmRinging) {
        static unsigned long lastBeep = 0;
        if (millis() - lastBeep > 1000) {
            lastBeep = millis();
            playSystemTone(2000, 400); 
        }
        if (encPressed || k0Pressed) {
            ui.alarmRinging = false;
            ui.lastAlarmDayTriggered = -1;
            stopSystemTone();
            drawAlarmScreen(true);
        }
        return;
    }

    bool changed = false;
    static int lastMode = -1;
    if (lastMode != MODE_ALARM) { 
        drawAlarmScreen(true); 
        lastMode = MODE_ALARM; 
    }

    if (encStep != 0) {
        if (ui.alarmSelectedField == 0) {
            settings.alarmHour = (settings.alarmHour + (encStep > 0 ? 1 : 23)) % 24;
            changed = true;
        } else if (ui.alarmSelectedField == 1) {
            settings.alarmMinute = (settings.alarmMinute + (encStep > 0 ? 1 : 59)) % 60;
            changed = true;
        } else if (ui.alarmSelectedField == 2) {
            settings.alarmEnabled = !settings.alarmEnabled;
            changed = true;
        }
        if (changed) ui.lastAlarmDayTriggered = -1;
    }
    if (encPressed) {
        ui.alarmSelectedField = (ui.alarmSelectedField + 1) % 3;
        changed = true;
    }
    if (k0Pressed) {
        saveSettings(); 
        ui.currentMode = MODE_MENU;
        lastMode = MODE_MENU; 
        return;
    }
    if (changed) {
        drawAlarmScreen(false);
        drawAlarmIcon();
    }
}

// --- DVD SCREEN ---

void initDvd() {
    ui.dvdInited = true;
    tft.fillScreen(Colors::BG);
    drawAlarmIcon();
    ui.dvdX = 80; ui.dvdY = 80; ui.dvdVX = 3; ui.dvdVY = 2;
    ui.lastDvdMs = millis();
    ui.dvdColorIndex = 0;
}

void drawDvdLogo(int x, int y, uint16_t c) {
    tft.fillRoundRect(x, y, ui.dvdW, ui.dvdH, 8, c);
    tft.drawRoundRect(x, y, ui.dvdW, ui.dvdH, 8, Colors::BG);
    printCenteredText("DVD", x, x + ui.dvdW, y + ui.dvdH/2 - 6, Colors::BG, c, 2);
}

void runDvd(int encStep, bool encPressed, bool k0Pressed) {
    if (!ui.dvdInited) {
        initDvd();
        drawDvdLogo(ui.dvdX, ui.dvdY, dvdPalette[ui.dvdColorIndex]);
    }
    
    if (k0Pressed) {
        ui.dvdInited = false;
        ui.currentMode = MODE_MENU;
        return;
    }
    
    if (encStep != 0) {
        int speed = abs(ui.dvdVX) + encStep;
        if (speed < 1) speed = 1; else if (speed > 8) speed = 8;
        ui.dvdVX = (ui.dvdVX >= 0 ? 1 : -1) * speed;
    }
    
    unsigned long now = millis();
    if (now - ui.lastDvdMs < 35) return;
    ui.lastDvdMs = now;

    tft.fillRoundRect(ui.dvdX, ui.dvdY, ui.dvdW, ui.dvdH, 8, Colors::BG);
    ui.dvdX += ui.dvdVX;
    ui.dvdY += ui.dvdVY;

    int left = 0, right = Screen::WIDTH - ui.dvdW;
    int top = 0, bottom = Screen::HEIGHT - ui.dvdH;
    bool hitX = false, hitY = false;

    if (ui.dvdX <= left) { ui.dvdX = left; ui.dvdVX = -ui.dvdVX; hitX = true; } 
    else if (ui.dvdX >= right) { ui.dvdX = right; ui.dvdVX = -ui.dvdVX; hitX = true; }
    if (ui.dvdY <= top) { ui.dvdY = top; ui.dvdVY = -ui.dvdVY; hitY = true; } 
    else if (ui.dvdY >= bottom) { ui.dvdY = bottom; ui.dvdVY = -ui.dvdVY; hitY = true; }

    if (hitX && hitY) {
        ui.dvdColorIndex = (ui.dvdColorIndex + 1) % 6;
        playSystemTone(1500, 80); 
    }
    drawDvdLogo(ui.dvdX, ui.dvdY, dvdPalette[ui.dvdColorIndex]);
}

// --- SETTINGS SCREEN ---

void drawSettingsScreen(bool full) {
    if (full) {
        tft.fillScreen(Colors::BG);
        drawAlarmIcon();
        ui.prevLedBarW = -1;
        ui.prevSpkBarW = -1;
    }

    int barX = 40; int barW = 240; int barH = 15;
    
    uint16_t ledTextColor = ST77XX_WHITE;
    uint16_t spkTextColor = ST77XX_WHITE;
    uint16_t graphTextColor = ST77XX_WHITE; // New color var
    uint16_t wifiTextColor = ST77XX_WHITE;
    uint16_t resetTextColor = ST77XX_WHITE;

    // Highlight logic
    if (ui.settingsSelectedRow == 0) ledTextColor = ui.settingsEditMode ? Colors::GREEN : Colors::BLUE;
    else if (ui.settingsSelectedRow == 1) spkTextColor = ui.settingsEditMode ? Colors::GREEN : Colors::BLUE;
    else if (ui.settingsSelectedRow == 2) graphTextColor = ui.settingsEditMode ? Colors::GREEN : Colors::BLUE; // New Row
    else if (ui.settingsSelectedRow == 3) wifiTextColor = Colors::BLUE; 
    else if (ui.settingsSelectedRow == 4) resetTextColor = ST77XX_RED; 
    
    // --- Row 0: LED (y=40) ---
    int yLed = 40;
    tft.setTextSize(2);
    tft.setTextColor(ledTextColor, Colors::BG);
    tft.setCursor(barX, yLed - 20); tft.print("LED Brightness");
    drawBarItem(barX, yLed, barW, barH, settings.ledBrightness, ui.prevLedBarW);

    // --- Row 1: Speaker (y=80) ---
    int ySpk = 80; // Moved up slightly to fit 5 items
    tft.setTextColor(spkTextColor, Colors::BG);
    tft.setCursor(barX, ySpk - 20); tft.print("Speaker Volume");
    drawBarItem(barX, ySpk, barW, barH, settings.speakerVol, ui.prevSpkBarW);

    // --- Row 2: Graph Duration (y=120) ---
    int yGraph = 120;
    tft.setTextColor(graphTextColor, Colors::BG);
    tft.setCursor(barX, yGraph - 20); tft.print("Graph Range");
    
    // Draw Text Box for Time
    tft.drawRect(barX - 1, yGraph - 1, barW + 2, barH + 12, ST77XX_WHITE); // Taller box for text
    tft.fillRect(barX, yGraph, barW, barH + 10, Colors::DARK);
    
    String rangeStr;
    if (settings.graphDuration < 60) rangeStr = String(settings.graphDuration) + " Min";
    else rangeStr = String(settings.graphDuration / 60) + " Hours";
    
    // Center the text inside the box
    int16_t bx, by; uint16_t bw, bh;
    tft.getTextBounds(rangeStr, 0, 0, &bx, &by, &bw, &bh);
    tft.setCursor(barX + (barW - bw)/2, yGraph + 4);
    tft.setTextColor(ST77XX_WHITE);
    tft.print(rangeStr);

    // --- Row 3: WiFi Setup (y=165) ---
    int yWifi = 165;
    if (ui.settingsSelectedRow == 3) {
        tft.fillRoundRect(barX, yWifi, barW, 25, 4, Colors::BLUE);
        tft.setTextColor(Colors::BG);
    } else {
        tft.fillRoundRect(barX, yWifi, barW, 25, 4, Colors::DARK);
        tft.setTextColor(ST77XX_WHITE);
    }
    String wifiText = "Setup WiFi";
    tft.getTextBounds(wifiText, 0, 0, &bx, &by, &bw, &bh);
    tft.setCursor(barX + (barW-bw)/2, yWifi + (25-bh)/2);
    tft.print(wifiText);

    // --- Row 4: Reset WiFi (y=200) ---
    int yReset = 200;
    if (ui.settingsSelectedRow == 4) {
        tft.fillRoundRect(barX, yReset, barW, 25, 4, ST77XX_RED);
        tft.setTextColor(Colors::BG);
    } else {
        tft.fillRoundRect(barX, yReset, barW, 25, 4, Colors::DARK);
        tft.setTextColor(ST77XX_WHITE);
    }
    String resetText = "Reset WiFi";
    tft.getTextBounds(resetText, 0, 0, &bx, &by, &bw, &bh);
    tft.setCursor(barX + (barW-bw)/2, yReset + (25-bh)/2);
    tft.print(resetText);
}

void runSettingsMenu(int encStep, bool encPressed, bool k0Pressed) {
    static int lastIndex = -1;
    
    // 1. Draw List Logic
    if (ui.settingsIndex != lastIndex) {
        drawGenericList(ui.settingsLabels, ui.SETTINGS_ITEMS, ui.settingsIndex, lastIndex, lastIndex == -1);
        lastIndex = ui.settingsIndex;
    }

    // 2. Navigation
    if (encStep != 0) {
        lastIndex = ui.settingsIndex; 
        ui.settingsIndex += encStep;
        if (ui.settingsIndex < 0) ui.settingsIndex = ui.SETTINGS_ITEMS - 1;
        if (ui.settingsIndex >= ui.SETTINGS_ITEMS) ui.settingsIndex = 0;
        
        drawGenericList(ui.settingsLabels, ui.SETTINGS_ITEMS, ui.settingsIndex, lastIndex, false);
        lastIndex = ui.settingsIndex; // Update immediately after draw
    }

    // 3. Selection
    if (encPressed) {
        // === FIX HERE: Reset lastIndex so it redraws when we return ===
        lastIndex = -1; 
        
        if (ui.settingsIndex == 0) { // LED
            ui.currentMode = MODE_SETTINGS_EDIT;
            ui.editId = 0;
            ui.prevVal = -1; 
            setLedState(true);
            tft.fillScreen(Colors::BG);
        } 
        else if (ui.settingsIndex == 1) { // Speaker
            ui.currentMode = MODE_SETTINGS_EDIT;
            ui.editId = 1;
            ui.prevVal = -1;
            tft.fillScreen(Colors::BG);
        }
        else if (ui.settingsIndex == 2) { // Graph Range
            ui.currentMode = MODE_SETTINGS_EDIT;
            ui.editId = 2;
            ui.prevVal = -1;
            tft.fillScreen(Colors::BG);
        }
        else if (ui.settingsIndex == 3) { // WiFi Setup
            ui.currentMode = MODE_WIFI_SETUP;
            ui.wifiSetupInited = false;
        }
        else if (ui.settingsIndex == 4) { // Reset WiFi
            tft.fillScreen(Colors::BG);
            tft.setTextColor(ST77XX_RED);
            tft.setTextSize(3);
            tft.setCursor(20, 100);
            tft.print("RESETTING...");
            playSystemTone(1000, 500);
            wm.resetSettings();
            ESP.restart();
        }
    }

    // 4. Exit
    if (k0Pressed) {
        saveSettings();
        lastIndex = -1; 
        ui.currentMode = MODE_MENU;
        // No drawing here either, runMenu will handle it
    }
}

void runSettingsEdit(int encStep, bool encPressed, bool k0Pressed) {
    // 1. Handle Value Changes
    bool valChanged = false;
    int currentVal = 0; 

    if (ui.editId == 0) { // LED
        if (encStep != 0) {
            int newVal = constrain(settings.ledBrightness + (encStep * 5), 0, 100);
            if (newVal != settings.ledBrightness) {
                settings.ledBrightness = newVal;
                setLedState(true); 
                valChanged = true;
            }
        }
        currentVal = settings.ledBrightness;
    } 
    else if (ui.editId == 1) { // Speaker
        if (encStep != 0) {
            int newVal = constrain(settings.speakerVol + (encStep * 5), 0, 100);
            if (newVal != settings.speakerVol) {
                settings.speakerVol = newVal;
                playSystemTone(2000, 20); 
                valChanged = true;
            }
        }
        currentVal = settings.speakerVol;
    }
    else if (ui.editId == 2) { // Graph Range
        if (encStep != 0) {
            // Find current index
            int oldIdx = 0;
            for(int i=0; i<GRAPH_RANGES_COUNT; i++) {
                if (settings.graphDuration == GRAPH_RANGES_MIN[i]) { oldIdx = i; break; }
            }
            
            // Calculate NEW index
            int newIdx = constrain(oldIdx + encStep, 0, GRAPH_RANGES_COUNT - 1);
            
            // Only update if it ACTUALLY changed
            if (newIdx != oldIdx) {
                settings.graphDuration = GRAPH_RANGES_MIN[newIdx];
                valChanged = true;
            }
        }
        currentVal = settings.graphDuration;
    }

    // 2. Initial Setup (Run ONLY once when entering the page)
    if (ui.prevVal == -1) {
        tft.fillScreen(Colors::BG);
        
        // Draw Header
        String title = (ui.editId==0) ? "LED Brightness" : (ui.editId==1) ? "Speaker Volume" : "Graph Range";
        printCenteredText(title, 0, Screen::WIDTH, 40, Colors::LIGHT, Colors::BG, 2);

        // Draw Bar Container (Only for LED/Speaker)
        if (ui.editId != 2) {
             int barW = 260; int barH = 15; int barX = (Screen::WIDTH - barW)/2; int barY = 160;
             tft.drawRect(barX-1, barY-1, barW+2, barH+2, ST77XX_WHITE);
             tft.fillRect(barX, barY, barW, barH, Colors::DARK); 
             
             // Draw Initial Green Bar
             int fillW = (int)((float)barW * (currentVal / 100.0f));
             if(fillW > 0) tft.fillRect(barX, barY, fillW, barH, Colors::GREEN);
        }
        
        // Force text update next block
        ui.prevVal = -999; 
        valChanged = true;
    }

    // 3. Update Screen (Run only if changed)
    if (valChanged) {
        tft.setTextSize(5);
        int16_t bx, by; uint16_t bw, bh;

        // --- GRAPH RANGE (Erase Old -> Draw New) ---
        if (ui.editId == 2) {
            // A. ERASE OLD VALUE (Draw previous string in Background Color)
            if (ui.prevVal != -999) {
                char oldBuf[16];
                if (ui.prevVal < 60) sprintf(oldBuf, "%dm", ui.prevVal);
                else sprintf(oldBuf, "%dh", ui.prevVal/60);
                
                tft.setTextColor(Colors::BG, Colors::BG); // Eraser
                tft.getTextBounds(oldBuf, 0, 0, &bx, &by, &bw, &bh);
                tft.setCursor((Screen::WIDTH - bw)/2, 90);
                tft.print(oldBuf);
            }

            // B. DRAW NEW VALUE
            char newBuf[16];
            if (currentVal < 60) sprintf(newBuf, "%dm", currentVal);
            else sprintf(newBuf, "%dh", currentVal/60);
            
            tft.setTextColor(ST77XX_WHITE, Colors::BG);
            tft.getTextBounds(newBuf, 0, 0, &bx, &by, &bw, &bh);
            tft.setCursor((Screen::WIDTH - bw)/2, 90);
            tft.print(newBuf);
        } 
        
        // --- LED / SPEAKER (Overwrite) ---
        else { 
            tft.setTextColor(ST77XX_WHITE, Colors::BG);
            char buf[16];
            sprintf(buf, "%3d%%", currentVal); 
            
            tft.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
            tft.setCursor((Screen::WIDTH - bw)/2, 90);
            tft.print(buf);

            // Update Bar
            if (ui.prevVal != -999) { 
                 int barW = 260; int barH = 15; int barX = (Screen::WIDTH - barW)/2; int barY = 160;
                 int oldW = (int)((float)barW * (ui.prevVal / 100.0f));
                 int newW = (int)((float)barW * (currentVal / 100.0f));
                 
                 if (newW != oldW) {
                     if (newW > oldW) tft.fillRect(barX + oldW, barY, newW - oldW, barH, Colors::GREEN);
                     else tft.fillRect(barX + newW, barY, oldW - newW, barH, Colors::DARK);
                 }
            }
        }
        
        ui.prevVal = currentVal;
    }

    // 4. Exit
    if (encPressed || k0Pressed) {
        setLedState(false); 
        ui.currentMode = MODE_SETTINGS;
        ui.settingsIndex = ui.editId; 
    }
}

// --- WIFI SETUP MODE ---

void runWiFiSetup(bool k0Pressed) {
    if (!ui.wifiSetupInited) {
        tft.fillScreen(Colors::BG);
        
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(20, 100); tft.print("AP: CyberClockSetup");
        tft.setCursor(20, 120); tft.print("IP: 192.168.4.1");
        
        // Non-blocking mode
        wm.setConfigPortalBlocking(false);
        wm.startConfigPortal("CyberClockSetup");
        
        ui.wifiSetupInited = true;
    }

    // Process WiFiManager
    wm.process();

    // Check for Cancel
    if (k0Pressed) {
        wm.stopConfigPortal();
        ui.currentMode = MODE_SETTINGS;
        ui.settingsInited = false; // Force redraw of settings
        return;
    }

    // Check for Success
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(Colors::BG);
        tft.setTextColor(Colors::GREEN); tft.setTextSize(2);
        tft.setCursor(20, 100); tft.print("Connected!");
        delay(100);
        
        syncTime();
        
        wm.stopConfigPortal();
        ui.currentMode = MODE_SETTINGS;
        ui.settingsInited = false;
    }
}

// ==========================================
//           SYSTEM LOOP CHECKERS
// ==========================================

void checkAlarmTrigger() {
    if (!settings.alarmEnabled || ui.alarmRinging) return;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    if (timeinfo.tm_hour == settings.alarmHour && timeinfo.tm_min == settings.alarmMinute && timeinfo.tm_sec == 0) {
        ui.alarmRinging = true;
        ui.lastAlarmDayTriggered = timeinfo.tm_mday;
        ui.currentMode = MODE_ALARM;
        drawAlarmRingingScreen();
    }
}

void updateAlertStateAndLED() {
    if (ui.currentMode == MODE_WIFI_SETUP) return; 
    
    if (ui.alarmRinging) ui.currentAlert = ALERT_ALARM;
    else if (env.eco2 > 1800) ui.currentAlert = ALERT_CO2;
    else ui.currentAlert = ALERT_NONE;

    unsigned long now = millis();
    if (ui.currentAlert == ALERT_NONE) {
        if (ui.currentMode != MODE_SETTINGS && ui.currentMode != MODE_SETTINGS_EDIT) {
             setLedState(false);
        }
        ui.ledState = false;
    } else {
        unsigned long interval = (ui.currentAlert == ALERT_ALARM) ? 120 : 250; 
        if (now - ui.lastLedToggleMs > interval) {
             ui.lastLedToggleMs = now;
             ui.ledState = !ui.ledState;
             setLedState(ui.ledState);
        }
    }

    if (ui.currentAlert == ALERT_CO2) {
        if (now - ui.lastCo2BlinkMs > 350) {
             ui.lastCo2BlinkMs = now;
             ui.co2BlinkOn = !ui.co2BlinkOn;
             playSystemTone(1800, 80); 
        }
    }
}

void checkStartupWiFi() {
    tft.fillScreen(Colors::BG);
    tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
    tft.setCursor(20, 100); tft.print("Checking WiFi...");

    // Disable the auto-AP feature.
    wm.setEnableConfigPortal(false);
    
    // Attempt connection
    bool connected = wm.autoConnect(); 
    
    if (connected) {
        tft.setTextColor(Colors::GREEN);
        tft.setCursor(20, 130); tft.print("Connected!");
        delay(100);
        syncTime();
        
        struct tm timeinfo;
        int retries = 0;
        while (!getLocalTime(&timeinfo) && retries < 5) {
            delay(500);
            retries++;
        }
    } else {
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(20, 130); tft.print("Offline Mode");
        delay(100);
        
        // Manual Time Init
        struct tm tm;
        tm.tm_year = 2024 - 1900;
        tm.tm_mon = 0;
        tm.tm_mday = 1;
        tm.tm_hour = 12;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        time_t t = mktime(&tm);
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, NULL);
    }
}

// ==========================================
//               MAIN
// ==========================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize History
    for(int i=0; i<EnvData::HIST_LEN; i++) {
        env.hTemp[i] = 0; env.hHum[i] = 0; env.hTVOC[i] = 0; env.hCO2[i] = 400; 
    }

    // Hardware Init
    pinMode(Pins::ENC_A,   INPUT_PULLUP);
    pinMode(Pins::ENC_B,   INPUT_PULLUP);
    pinMode(Pins::ENC_BTN, INPUT_PULLUP);
    pinMode(Pins::KEY0,    INPUT_PULLUP);
    
    ledcSetup(PWM::CH_LED, 5000, 8);
    ledcAttachPin(Pins::LED, PWM::CH_LED);

    ledcSetup(PWM::CH_BUZZ, 2000, 8);
    ledcAttachPin(Pins::BUZZ, PWM::CH_BUZZ);

    loadSettings();

    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
    SPI.begin(Pins::TFT_SCLK, -1, Pins::TFT_MOSI, Pins::TFT_CS); 

    tft.init(Screen::HEIGHT, Screen::WIDTH); 
    tft.setRotation(1); 
    tft.invertDisplay(false);
    tft.fillScreen(Colors::BG);

    checkStartupWiFi();

    if (!aht.begin()) Serial.println("AHT21 not found");
    if (!ens160.begin()) Serial.println("ENS160 begin FAIL");
    else ens160.setMode(ENS160_OPMODE_STD);

    updateEnvSensors(true);

    // Initial Screen
    initClockStaticUI();
    drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
    drawEnvDynamic();
}

void loop() {
    // Input
    int encStep = readEncoderStep();
    bool encPressed = checkButtonPressed(Pins::ENC_BTN, input.lastEncBtn, input.lastEncBtnMs);
    bool k0Pressed  = checkButtonPressed(Pins::KEY0,    input.lastKey0,   input.lastKey0Ms);

    // System Checks
    checkAlarmTrigger();
    updateAlertStateAndLED();

    // Mode Dispatch
    switch (ui.currentMode) {
        case MODE_MENU:       runMenu(encStep, encPressed, k0Pressed); break;
        case MODE_CLOCK:      runClock(k0Pressed); break;
        case MODE_POMODORO:   runPomodoro(encStep, encPressed, k0Pressed); break;
        case MODE_ALARM:      runAlarm(encStep, encPressed, k0Pressed); break;
        case MODE_DVD:        runDvd(encStep, encPressed, k0Pressed); break;
        case MODE_SETTINGS:       runSettingsMenu(encStep, encPressed, k0Pressed); break;
        case MODE_SETTINGS_EDIT:  runSettingsEdit(encStep, encPressed, k0Pressed); break;
        case MODE_WIFI_SETUP: runWiFiSetup(k0Pressed); break;
    }
}