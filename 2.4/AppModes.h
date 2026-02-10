#ifndef APPMODES_H
#define APPMODES_H

#include "Mode.h"
#include "Globals.h"
#include "Graphics.h"
#include "Hardware.h"
#include "StateManager.h"

// --- Menu Mode ---
class MenuMode : public Mode {
private:
    int index = 0;
    static const int ITEMS = 5;
    const char* labels[ITEMS] = { "Monitor", "Pomodoro", "Alarm", "DVD", "Settings" };
    int lastIndex = -1;
public:
    void enter() override;
    void loop() override;
};

// --- Clock Mode ---
class ClockMode : public Mode {
private:
    int prevSecond = -1;
    String prevTimeStr = "";
public:
    void enter() override;
    void loop() override;
};

// --- Pomodoro Mode ---
class PomodoroMode : public Mode {
private:
    // Logic moved from global struct to here
    PomodoroState state = POMO_SET_WORK;
    PomoPhase phase = PHASE_WORK;
    int currentCycle = 1;
    unsigned long startMillis = 0;
    unsigned long pausedMillis = 0;
    unsigned long durationMs = 0;
    int prevVal = -1;
    int prevBarWidth = -1;
    String prevLabel = "";
    String prevTimeStr = "";
    
    void drawScreen(bool force);
    void updateValue(int val);

public:
    void enter() override;
    void loop() override;
};

// --- Alarm Mode ---
class AlarmMode : public Mode {
private:
    int selectedField = 0;
    bool ringing = false;
    unsigned long lastBeep = 0;
    
    void draw(bool full);

public:
    AlarmMode(bool isRinging = false); // Constructor to handle trigger
    void enter() override;
    void loop() override;
};

// --- DVD Mode ---
class DvdMode : public Mode {
private:
    int x = 80, y = 80, vx = 3, vy = 2;
    int w = 80, h = 30;
    unsigned long lastMs = 0;
    int colorIndex = 0;

public:
    void enter() override;
    void loop() override;
};

// --- Settings List Mode ---
class SettingsMode : public Mode {
private:
    int index = 0;
    static const int ITEMS = 4;
    const char* labels[ITEMS] = { "LED Brightness", "Speaker Volume", "Graph Range", "WiFi" };
    int lastIndex = -1;

public:
    void enter() override;
    void loop() override;
};

// --- Settings Edit Mode ---
class SettingsEditMode : public Mode {
private:
    int editId; // 0=LED, 1=Spk, 2=Graph
    int currentVal;
    int prevVal = -1;
    int prevBarWidth = -1;
    void drawValue();

public:
    SettingsEditMode(int id);
    void enter() override;
    void loop() override;
};

// --- WiFi Menu Mode ---
class WiFiMenuMode : public Mode {
private:
    int index = 0;
    int lastIndex = -1;
    bool redraw = true;

public:
    void enter() override;
    void loop() override;
};

// --- WiFi Setup Mode ---
class WiFiSetupMode : public Mode {
private:
    bool inited = false;
public:
    void enter() override;
    void loop() override;
};

void checkAlarmTrigger();
void checkStartupWiFi();

#endif