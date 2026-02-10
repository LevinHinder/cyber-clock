#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

enum UIMode { 
    MODE_MENU = 0, 
    MODE_CLOCK, 
    MODE_POMODORO, 
    MODE_ALARM, 
    MODE_DVD, 
    MODE_SETTINGS,      
    MODE_SETTINGS_EDIT, 
    MODE_WIFI_MENU,
    MODE_WIFI_SETUP,
    MODE_WIFI_RESET_CONFIRM
};

enum AlertLevel { ALERT_NONE = 0, ALERT_CO2, ALERT_ALARM };
enum PomodoroState { POMO_SET_WORK = 0, POMO_SET_SHORT, POMO_SET_LONG, POMO_SET_CYCLES, POMO_READY, POMO_RUNNING, POMO_PAUSED, POMO_DONE };
enum PomoPhase { PHASE_WORK, PHASE_SHORT, PHASE_LONG };

struct AppSettings {
    int ledBrightness = 100; 
    int speakerVol = 100;
    int graphDuration = 180;
    uint8_t alarmHour = 6;
    uint8_t alarmMinute = 0;
    bool alarmEnabled = false;
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
    int menuIndex = 0;
    static const int MENU_ITEMS = 5; 
    const char* menuLabels[MENU_ITEMS] = { "Monitor", "Pomodoro", "Alarm", "DVD", "Settings" };
    int settingsIndex = 0;
    static const int SETTINGS_ITEMS = 4;
    const char* settingsLabels[SETTINGS_ITEMS] = { "LED Brightness", "Speaker Volume", "Graph Range", "WiFi" };
    int editId = -1; 
    int prevVal = -1; 
    bool alarmRinging = false;
    int alarmSelectedField = 0; 
    int lastAlarmDayTriggered = -1;
    int settingsSelectedRow = 0;
    static const int SETTINGS_ROWS = 4; 
    bool settingsEditMode = false;
    bool settingsInited = false;
    int prevLedBarW = -1;
    int prevSpkBarW = -1;
    int wifiMenuIndex = 0; 
    bool wifiMenuRedraw = true;
    int wifiResetConfirmIndex = 1; 
    bool wifiResetRedraw = true;
    bool wifiSetupInited = false;
    int prevSecond = -1;
    String prevTimeStr = "";
    bool dvdInited = false;
    int dvdX, dvdY, dvdVX = 2, dvdVY = 1;
    int dvdW = 80, dvdH = 30; 
    unsigned long lastDvdMs = 0;
    int dvdColorIndex = 0;
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
    bool key0Consumed = false;
};

#endif