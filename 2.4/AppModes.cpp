#include "AppModes.h"

// ================= CLOCK MODE =================
void ClockMode::enter() {
    ui.currentMode = MODE_CLOCK;
    initClockStaticUI();
    prevTimeStr = "";
    updateEnvSensors(true);
    drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
    drawEnvDynamic();
}

void ClockMode::loop() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        int sec = timeinfo.tm_sec;
        if (sec != prevSecond) {
            prevSecond = sec;
            drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
            if (sec % 5 == 0) {
                updateEnvSensors(true);
                drawEnvDynamic();
            } else {
                updateEnvSensors(false);
            }
        }
    }
    if (Input.backPressed)
        State.switchMode(new MenuMode());
}

// ================= MENU MODE =================
void MenuMode::enter() {
    // No specific draw here, list handles it
}

void MenuMode::loop() {
    if (index != lastIndex) {
        drawGenericList(labels, ITEMS, index, lastIndex, lastIndex == -1);
        lastIndex = index;
    }
    if (Input.encStep != 0) {
        lastIndex = index;
        index += Input.encStep;
        if (index < 0)
            index = ITEMS - 1;
        if (index >= ITEMS)
            index = 0;
        drawGenericList(labels, ITEMS, index, lastIndex, false);
    }
    if (Input.encPressed) {
        if (index == 0)
            State.switchMode(new ClockMode());
        else if (index == 1)
            State.switchMode(new PomodoroMode());
        else if (index == 2)
            State.switchMode(new AlarmMode(false));
        else if (index == 3)
            State.switchMode(new DvdMode());
        else if (index == 4)
            State.switchMode(new SettingsMode());
    }
    if (Input.backPressed)
        State.switchMode(new ClockMode());
}

// ================= POMODORO MODE =================
void PomodoroMode::enter() {
    state = POMO_SET_WORK;
    prevVal = -1;
    UI::drawHeader("Set Work");
    updateValue(settings.pomoWorkMin);
}

void PomodoroMode::updateValue(int value) {
    if (value == prevVal)
        return;
    prevVal = value;
    int numY = 100;
    int numW = 100;
    int startX = (Screen::WIDTH - numW) / 2;
    tft.fillRect(startX, numY, numW, 50, Colors::BG);
    UI::textCentered(String(value), numY, 6, ST77XX_WHITE);
}

void PomodoroMode::drawScreen(bool force) {
    if (force) {
        UI::clear();
        prevBarWidth = -1;
        prevLabel = "";
        prevTimeStr = "";
        char cycleBuf[20];
        sprintf(cycleBuf, "Cycle: %d/%d", currentCycle, settings.pomoCycles);
        UI::textCentered(cycleBuf, 190, 2, ST77XX_WHITE);
    }
    String labelStr = "";
    uint16_t labelColor = Colors::LIGHT;
    if (state == POMO_PAUSED) {
        labelStr = "Paused";
        labelColor = ST77XX_YELLOW;
    } else if (phase == PHASE_WORK) {
        labelStr = "Get to Work!";
        labelColor = Colors::LIGHT;
    } else if (phase == PHASE_SHORT) {
        labelStr = "Short Break";
        labelColor = Colors::GREEN;
    } else {
        labelStr = "Long Break";
        labelColor = Colors::BLUE;
    }

    if (labelStr != prevLabel) {
        tft.fillRect(0, 30, Screen::WIDTH, 20, Colors::BG);
        UI::textCentered(labelStr, 30, 2, labelColor);
        prevLabel = labelStr;
    }

    unsigned long elapsed = 0;
    if (state == POMO_RUNNING)
        elapsed = millis() - startMillis;
    else if (state == POMO_PAUSED)
        elapsed = pausedMillis - startMillis;
    if (elapsed > durationMs)
        elapsed = durationMs;
    float progress = (durationMs > 0) ? (float)elapsed / durationMs : 0.0f;

    UI::drawBar(20, 225, 280, 10, (int)(progress * 100), Colors::GREEN,
                prevBarWidth);

    unsigned long remain = durationMs - elapsed;
    char buf[8];
    sprintf(buf, "%02d:%02d", (int)(remain / 60000UL),
            (int)((remain / 1000UL) % 60));
    String timeStr = String(buf);
    uint16_t timeColor = (state == POMO_PAUSED) ? Colors::LIGHT : ST77XX_WHITE;
    if (timeStr != prevTimeStr) {
        UI::textCentered(timeStr, 90, 6, timeColor);
        prevTimeStr = timeStr;
    }
}

void PomodoroMode::loop() {
    if (state < POMO_READY) {
        if (Input.encStep != 0) {
            if (state == POMO_SET_WORK) {
                settings.pomoWorkMin =
                    constrain(settings.pomoWorkMin + Input.encStep, 1, 90);
                updateValue(settings.pomoWorkMin);
            } else if (state == POMO_SET_SHORT) {
                settings.pomoShortMin =
                    constrain(settings.pomoShortMin + Input.encStep, 1, 30);
                updateValue(settings.pomoShortMin);
            } else if (state == POMO_SET_LONG) {
                settings.pomoLongMin =
                    constrain(settings.pomoLongMin + Input.encStep, 1, 60);
                updateValue(settings.pomoLongMin);
            } else if (state == POMO_SET_CYCLES) {
                settings.pomoCycles =
                    constrain(settings.pomoCycles + Input.encStep, 1, 10);
                updateValue(settings.pomoCycles);
            }
        }
        if (Input.encPressed) {
            prevVal = -1;
            if (state == POMO_SET_WORK) {
                state = POMO_SET_SHORT;
                UI::drawHeader("Short Break");
                updateValue(settings.pomoShortMin);
            } else if (state == POMO_SET_SHORT) {
                state = POMO_SET_LONG;
                UI::drawHeader("Long Break");
                updateValue(settings.pomoLongMin);
            } else if (state == POMO_SET_LONG) {
                state = POMO_SET_CYCLES;
                UI::drawHeader("Set Cycles");
                updateValue(settings.pomoCycles);
            } else if (state == POMO_SET_CYCLES) {
                state = POMO_RUNNING;
                phase = PHASE_WORK;
                currentCycle = 1;
                durationMs = settings.pomoWorkMin * 60UL * 1000UL;
                startMillis = millis();
                drawScreen(true);
            }
        }
    } else if (state == POMO_RUNNING || state == POMO_PAUSED) {
        if (Input.encPressed) {
            if (state == POMO_RUNNING) {
                state = POMO_PAUSED;
                pausedMillis = millis();
            } else {
                startMillis += (millis() - pausedMillis);
                state = POMO_RUNNING;
            }
        }
        if (state == POMO_RUNNING) {
            unsigned long elapsed = millis() - startMillis;
            if (elapsed >= durationMs) {
                setLedState(true);
                playSystemTone(2000, 1500);
                setLedState(false);
                if (phase == PHASE_WORK) {
                    if (currentCycle < settings.pomoCycles) {
                        phase = PHASE_SHORT;
                        durationMs = settings.pomoShortMin * 60UL * 1000UL;
                    } else {
                        phase = PHASE_LONG;
                        durationMs = settings.pomoLongMin * 60UL * 1000UL;
                    }
                } else if (phase == PHASE_SHORT) {
                    phase = PHASE_WORK;
                    currentCycle++;
                    durationMs = settings.pomoWorkMin * 60UL * 1000UL;
                } else {
                    phase = PHASE_WORK;
                    currentCycle = 1;
                    durationMs = settings.pomoWorkMin * 60UL * 1000UL;
                }
                startMillis = millis();
                drawScreen(true);
            } else
                drawScreen(false);
        } else
            drawScreen(false);
    }
    if (Input.backPressed) {
        saveSettings();
        State.switchMode(new MenuMode());
    }
}

// ================= ALARM MODE =================
AlarmMode::AlarmMode(bool isRinging) : ringing(isRinging) {}

void AlarmMode::enter() {
    if (ringing) {
        UI::clear(ST77XX_RED);
        UI::text("ALARM!", 60, 100, 4, ST77XX_WHITE, ST77XX_RED);
    } else {
        draw(true);
    }
}

void AlarmMode::draw(bool full) {
    if (full) {
        UI::clear();
        UI::text("Status:", 50, 165, 3, ST77XX_WHITE);
    }
    char hBuf[3], mBuf[3];
    sprintf(hBuf, "%02d", settings.alarmHour);
    sprintf(mBuf, "%02d", settings.alarmMinute);
    int16_t x1, y1;
    uint16_t w, h;
    tft.setTextSize(6);
    tft.getTextBounds("88:88", 0, 0, &x1, &y1, &w, &h);
    int x = (Screen::WIDTH - w) / 2;
    tft.setCursor(x, 60);
    tft.setTextColor(selectedField == 0 ? Colors::LIGHT : ST77XX_WHITE,
                     Colors::BG);
    tft.print(hBuf);
    tft.setTextColor(ST77XX_WHITE, Colors::BG);
    tft.print(":");
    tft.setTextColor(selectedField == 1 ? Colors::LIGHT : ST77XX_WHITE,
                     Colors::BG);
    tft.print(mBuf);
    uint16_t enColor =
        selectedField == 2
            ? Colors::LIGHT
            : (settings.alarmEnabled ? Colors::GREEN : ST77XX_RED);
    UI::text(settings.alarmEnabled ? "ON " : "OFF", 180, 165, 3, enColor);
}

void AlarmMode::loop() {
    if (ringing) {
        if (millis() - lastBeep > 1000) {
            lastBeep = millis();
            playSystemTone(2000, 400);
        }
        if (Input.encPressed || Input.backPressed) {
            ringing = false;
            ui.lastAlarmDayTriggered = -1;
            stopSystemTone();
            draw(true);
        }
        return;
    }
    bool changed = false;
    if (Input.encStep != 0) {
        if (selectedField == 0)
            settings.alarmHour =
                (settings.alarmHour + (Input.encStep > 0 ? 1 : 23)) % 24;
        else if (selectedField == 1)
            settings.alarmMinute =
                (settings.alarmMinute + (Input.encStep > 0 ? 1 : 59)) % 60;
        else if (selectedField == 2)
            settings.alarmEnabled = !settings.alarmEnabled;
        changed = true;
        ui.lastAlarmDayTriggered = -1;
    }
    if (Input.encPressed) {
        selectedField = (selectedField + 1) % 3;
        changed = true;
    }
    if (Input.backPressed) {
        saveSettings();
        State.switchMode(new MenuMode());
        return;
    }
    if (changed)
        draw(false);
}

// ================= DVD MODE =================
void DvdMode::enter() {
    initDvd();
    drawDvdLogo(x, y, dvdPalette[colorIndex]);
}

void DvdMode::loop() {
    if (Input.backPressed) {
        State.switchMode(new MenuMode());
        return;
    }
    if (Input.encStep != 0) {
        int speed = abs(vx) + Input.encStep;
        if (speed < 1)
            speed = 1;
        else if (speed > 8)
            speed = 8;
        vx = (vx >= 0 ? 1 : -1) * speed;
    }
    unsigned long now = millis();
    if (now - lastMs < 35)
        return;
    lastMs = now;
    tft.fillRoundRect(x, y, w, h, 8, Colors::BG);
    x += vx;
    y += vy;
    bool hit = false;
    if (x <= 0) {
        x = 0;
        vx = -vx;
        hit = true;
    } else if (x >= Screen::WIDTH - w) {
        x = Screen::WIDTH - w;
        vx = -vx;
        hit = true;
    }
    if (y <= 0) {
        y = 0;
        vy = -vy;
        hit = true;
    } else if (y >= Screen::HEIGHT - h) {
        y = Screen::HEIGHT - h;
        vy = -vy;
        hit = true;
    }
    if (hit) {
        colorIndex = (colorIndex + 1) % 6;
        playSystemTone(1500, 80);
    }
    drawDvdLogo(x, y, dvdPalette[colorIndex]);
}

// ================= SETTINGS MODE =================
void SettingsMode::enter() { ui.currentMode = MODE_SETTINGS; }

void SettingsMode::loop() {
    if (index != lastIndex) {
        drawGenericList(labels, ITEMS, index, lastIndex, lastIndex == -1);
        lastIndex = index;
    }
    if (Input.encStep != 0) {
        lastIndex = index;
        index += Input.encStep;
        if (index < 0)
            index = ITEMS - 1;
        if (index >= ITEMS)
            index = 0;
        drawGenericList(labels, ITEMS, index, lastIndex, false);
    }
    if (Input.encPressed) {
        if (index < 3)
            State.switchMode(new SettingsEditMode(index));
        else
            State.switchMode(new WiFiMenuMode());
    }
    if (Input.backPressed) {
        saveSettings();
        State.switchMode(new MenuMode());
    }
}

// ================= SETTINGS EDIT MODE =================
SettingsEditMode::SettingsEditMode(int id) : editId(id) {
    if (id == 0)
        currentVal = settings.ledBrightness;
    else if (id == 1)
        currentVal = settings.speakerVol;
    else
        currentVal = settings.graphDuration;
}

void SettingsEditMode::enter() {
    ui.currentMode = MODE_SETTINGS_EDIT;

    String title = (editId == 0)   ? "LED Brightness"
                   : (editId == 1) ? "Speaker Volume"
                                   : "Graph Range";
    UI::drawHeader(title);

    if (editId == 0)
        setLedState(true);

    prevVal = -1;
    prevBarWidth = -1;

    drawValue();
    prevVal = currentVal;
}

void SettingsEditMode::drawValue() {
    int textY = 90;

    if (editId == 2) {
        String s;
        if (currentVal < 60)
            s = String(currentVal) + "m";
        else
            s = String(currentVal / 60) + "h";
        while (s.length() < 6) {
            if (s.length() % 2 == 0)
                s = s + " ";
            else
                s = " " + s;
        }
        UI::textCentered(s, textY, 5, ST77XX_WHITE, Colors::BG);
    } else {
        char buf[16];
        sprintf(buf, "%3d%%", currentVal);
        UI::textCentered(String(buf), textY, 5, ST77XX_WHITE, Colors::BG);

        int barW = 260;
        int barH = 15;
        int barX = (Screen::WIDTH - barW) / 2;
        int barY = 160;

        if (prevBarWidth == -1) {
            tft.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, ST77XX_WHITE);
            tft.fillRect(barX, barY, barW, barH, Colors::DARK);
        }

        UI::drawBar(barX, barY, barW, barH, currentVal, Colors::GREEN,
                    prevBarWidth);
    }
}

void SettingsEditMode::loop() {
    bool valChanged = false;

    if (Input.encStep != 0) {
        if (editId == 0) {
            settings.ledBrightness =
                constrain(settings.ledBrightness + (Input.encStep * 5), 0, 100);
            currentVal = settings.ledBrightness;
            setLedState(true);
            valChanged = true;
        } else if (editId == 1) {
            settings.speakerVol =
                constrain(settings.speakerVol + (Input.encStep * 5), 0, 100);
            playSystemTone(2000, 20);
            currentVal = settings.speakerVol;
            valChanged = true;
        } else {
            int oldIdx = 0;
            for (int i = 0; i < GRAPH_RANGES_COUNT; i++)
                if (settings.graphDuration == GRAPH_RANGES_MIN[i]) {
                    oldIdx = i;
                    break;
                }
            int newIdx =
                constrain(oldIdx + Input.encStep, 0, GRAPH_RANGES_COUNT - 1);
            settings.graphDuration = GRAPH_RANGES_MIN[newIdx];
            currentVal = settings.graphDuration;
            valChanged = true;
        }
    }

    if (valChanged) {
        drawValue();
        prevVal = currentVal;
    }

    if (Input.encPressed || Input.backPressed) {
        setLedState(false);
        State.switchMode(new SettingsMode());
    }
}

// ================= WIFI MODES (Condensed) =================
void WiFiMenuMode::enter() {
    UI::drawHeader("Current Network");
    String status =
        (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Not Connected";
    UI::textCentered(status, 70, 3,
                     (WiFi.status() == WL_CONNECTED) ? Colors::GREEN
                                                     : ST77XX_RED);
}
void WiFiMenuMode::loop() {
    if (index != lastIndex) {
        const char *opts[] = {"Setup", "Reset"};
        for (int i = 0; i < 2; i++) {
            int y = 140 + (i * 45);
            bool sel = (i == index);
            tft.fillRoundRect(40, y, 240, 35, 6,
                              sel ? ((i == 1) ? ST77XX_RED : Colors::ACCENT)
                                  : Colors::BG);
            if (!sel)
                tft.drawRoundRect(40, y, 240, 35, 6, Colors::DARK);
            UI::textCenteredX(opts[i], 40, 280, y + 10, 2,
                              sel ? Colors::BG : ST77XX_WHITE);
        }
        lastIndex = index;
    }
    if (Input.encStep != 0) {
        index = (index + Input.encStep);
        if (index < 0)
            index = 1;
        if (index > 1)
            index = 0;
    }
    if (Input.encPressed) {
        if (index == 0)
            State.switchMode(new WiFiSetupMode());
        else {
            UI::clear();
            UI::textCentered("Reset WiFi?", 90, 2, ST77XX_WHITE);
            wm.resetSettings();
            delay(500);
            ESP.restart();
        }
    }
    if (Input.backPressed)
        State.switchMode(new SettingsMode());
}

void WiFiSetupMode::enter() {
    UI::clear();
    UI::textCentered("WiFi Setup", 30, 3, ST77XX_WHITE);
    UI::text("Connect to: CyberClockSetup", 20, 70, 2, ST77XX_WHITE);
    UI::text("IP: 192.168.4.1", 20, 130, 2, ST77XX_WHITE);
    wm.setConfigPortalBlocking(false);
    wm.startConfigPortal("CyberClockSetup");
}
void WiFiSetupMode::loop() {
    wm.process();
    if (Input.backPressed) {
        wm.stopConfigPortal();
        State.switchMode(new WiFiMenuMode());
    }
    if (WiFi.status() == WL_CONNECTED) {
        UI::clear();
        UI::textCentered("Connected!", 100, 3, Colors::GREEN);
        delay(1500);
        wm.stopConfigPortal();
        syncTime();
        State.switchMode(new SettingsMode());
    }
}

void checkAlarmTrigger() {
    if (!settings.alarmEnabled || ui.alarmRinging)
        return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    if (timeinfo.tm_hour == settings.alarmHour &&
        timeinfo.tm_min == settings.alarmMinute && timeinfo.tm_sec == 0) {

        ui.alarmRinging = true;
        ui.lastAlarmDayTriggered = timeinfo.tm_mday;

        State.switchMode(new AlarmMode(true));
    }
}

void checkStartupWiFi() {
    UI::clear();
    UI::text("Checking WiFi...", 20, 100, 2, ST77XX_WHITE);

    wm.setEnableConfigPortal(false);
    bool connected = wm.autoConnect();

    if (connected) {
        UI::text("Connected!", 20, 130, 2, Colors::GREEN);
        delay(100);
        syncTime();

        struct tm timeinfo;
        int retries = 0;
        while (!getLocalTime(&timeinfo) && retries < 5) {
            delay(500);
            retries++;
        }
    } else {
        UI::text("Offline Mode", 20, 130, 2, ST77XX_RED);
        delay(100);

        struct tm tm;
        tm.tm_year = 2024 - 1900;
        tm.tm_mon = 0;
        tm.tm_mday = 1;
        tm.tm_hour = 12;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        time_t t = mktime(&tm);
        struct timeval now = {.tv_sec = t, .tv_usec = 0};
        settimeofday(&now, NULL);
    }
}