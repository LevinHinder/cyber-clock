#include "Modes.h"

void runClock() {
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
    if (Input.backPressed) {
        ui.currentMode = MODE_MENU;
    }
}

void runMenu() {
    static int lastMenuIndex = -1;
    if (ui.menuIndex != lastMenuIndex) {
        drawGenericList(ui.menuLabels, ui.MENU_ITEMS, ui.menuIndex, lastMenuIndex, lastMenuIndex == -1);
        lastMenuIndex = ui.menuIndex;
    }
    if (Input.encStep != 0) {
        lastMenuIndex = ui.menuIndex; 
        ui.menuIndex += Input.encStep;
        if (ui.menuIndex < 0) ui.menuIndex = ui.MENU_ITEMS - 1;
        if (ui.menuIndex >= ui.MENU_ITEMS) ui.menuIndex = 0;
        drawGenericList(ui.menuLabels, ui.MENU_ITEMS, ui.menuIndex, lastMenuIndex, false);
    }
    if (Input.encPressed) {
        lastMenuIndex = -1; 
        if (ui.menuIndex == 0) { 
             ui.currentMode = MODE_CLOCK;
             initClockStaticUI();
             ui.prevTimeStr = "";
             updateEnvSensors(true); 
             drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
             drawEnvDynamic();
        } else if (ui.menuIndex == 1) { 
             ui.currentMode = MODE_POMODORO;
             pomo.state = POMO_SET_WORK;
             pomo.prevVal = -1;
             UI::drawHeader("Set Work");
             updatePomodoroValue(settings.pomoWorkMin);
        } else if (ui.menuIndex == 2) { 
             ui.currentMode = MODE_ALARM;
             ui.alarmSelectedField = 0;
             UI::clear();
        } else if (ui.menuIndex == 3) { 
             ui.currentMode = MODE_DVD;
             ui.dvdInited = false;
        } else if (ui.menuIndex == 4) { 
             ui.currentMode = MODE_SETTINGS;
             ui.settingsIndex = 0;
        }
    }
    if (Input.backPressed) {
        lastMenuIndex = -1;
        ui.currentMode = MODE_CLOCK;
        initClockStaticUI();
        ui.prevTimeStr = "";
        updateEnvSensors(true); 
        drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
        drawEnvDynamic(); 
    }
}

void runPomodoro() {
    if (pomo.state < POMO_READY) {
        if (Input.encStep != 0) {
            if (pomo.state == POMO_SET_WORK) {
                settings.pomoWorkMin = constrain(settings.pomoWorkMin + Input.encStep, 1, 90);
                updatePomodoroValue(settings.pomoWorkMin);
            } else if (pomo.state == POMO_SET_SHORT) {
                settings.pomoShortMin = constrain(settings.pomoShortMin + Input.encStep, 1, 30);
                updatePomodoroValue(settings.pomoShortMin);
            } else if (pomo.state == POMO_SET_LONG) {
                settings.pomoLongMin = constrain(settings.pomoLongMin + Input.encStep, 1, 60);
                updatePomodoroValue(settings.pomoLongMin);
            } else if (pomo.state == POMO_SET_CYCLES) {
                settings.pomoCycles = constrain(settings.pomoCycles + Input.encStep, 1, 10);
                updatePomodoroValue(settings.pomoCycles);
            }
        }
        if (Input.encPressed) {
            pomo.prevVal = -1;
            if (pomo.state == POMO_SET_WORK) {
                pomo.state = POMO_SET_SHORT;
                UI::drawHeader("Short Break");
                updatePomodoroValue(settings.pomoShortMin);
            } else if (pomo.state == POMO_SET_SHORT) {
                pomo.state = POMO_SET_LONG;
                UI::drawHeader("Long Break");
                updatePomodoroValue(settings.pomoLongMin);
            } else if (pomo.state == POMO_SET_LONG) {
                pomo.state = POMO_SET_CYCLES;
                UI::drawHeader("Set Cycles");
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
        if (Input.encPressed) {
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
            if (elapsed >= pomo.durationMs) {
                setLedState(true);
                playSystemTone(2000, 1500);
                setLedState(false);
                if (pomo.phase == PHASE_WORK) {
                    if (pomo.currentCycle < settings.pomoCycles) {
                        pomo.phase = PHASE_SHORT;
                        pomo.durationMs = settings.pomoShortMin * 60UL * 1000UL;
                    } else {
                        pomo.phase = PHASE_LONG;
                        pomo.durationMs = settings.pomoLongMin * 60UL * 1000UL;
                    }
                } else if (pomo.phase == PHASE_SHORT) {
                    pomo.phase = PHASE_WORK;
                    pomo.currentCycle++;
                    pomo.durationMs = settings.pomoWorkMin * 60UL * 1000UL;
                } else if (pomo.phase == PHASE_LONG) {
                    pomo.phase = PHASE_WORK; 
                    pomo.currentCycle = 1; 
                    pomo.durationMs = settings.pomoWorkMin * 60UL * 1000UL; 
                }
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
        if (Input.encPressed) {
            pomo.state = POMO_SET_WORK;
            pomo.prevVal = -1;
            UI::drawHeader("Set Work");
            updatePomodoroValue(settings.pomoWorkMin);
        }
    }
    if (Input.backPressed) {
        saveSettings(); 
        ui.currentMode = MODE_MENU;
    }
}

void runAlarm() {
    if (ui.alarmRinging) {
        static unsigned long lastBeep = 0;
        if (millis() - lastBeep > 1000) {
            lastBeep = millis();
            playSystemTone(2000, 400); 
        }
        if (Input.encPressed || Input.backPressed) {
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
    if (Input.encStep != 0) {
        if (ui.alarmSelectedField == 0) {
            settings.alarmHour = (settings.alarmHour + (Input.encStep > 0 ? 1 : 23)) % 24;
            changed = true;
        } else if (ui.alarmSelectedField == 1) {
            settings.alarmMinute = (settings.alarmMinute + (Input.encStep > 0 ? 1 : 59)) % 60;
            changed = true;
        } else if (ui.alarmSelectedField == 2) {
            settings.alarmEnabled = !settings.alarmEnabled;
            changed = true;
        }
        if (changed) ui.lastAlarmDayTriggered = -1;
    }
    if (Input.encPressed) {
        ui.alarmSelectedField = (ui.alarmSelectedField + 1) % 3;
        changed = true;
    }
    if (Input.backPressed) {
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

void runDvd() {
    if (!ui.dvdInited) {
        initDvd();
        drawDvdLogo(ui.dvdX, ui.dvdY, dvdPalette[ui.dvdColorIndex]);
    }
    if (Input.backPressed) {
        ui.dvdInited = false;
        ui.currentMode = MODE_MENU;
        return;
    }
    if (Input.encStep != 0) {
        int speed = abs(ui.dvdVX) + Input.encStep;
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

void runSettingsMenu() {
    static int lastIndex = -1;
    if (ui.settingsIndex != lastIndex) {
        drawGenericList(ui.settingsLabels, ui.SETTINGS_ITEMS, ui.settingsIndex, lastIndex, lastIndex == -1);
        lastIndex = ui.settingsIndex;
    }
    if (Input.encStep != 0) {
        lastIndex = ui.settingsIndex; 
        ui.settingsIndex += Input.encStep;
        if (ui.settingsIndex < 0) ui.settingsIndex = ui.SETTINGS_ITEMS - 1;
        if (ui.settingsIndex >= ui.SETTINGS_ITEMS) ui.settingsIndex = 0;
        drawGenericList(ui.settingsLabels, ui.SETTINGS_ITEMS, ui.settingsIndex, lastIndex, false);
        lastIndex = ui.settingsIndex; 
    }
    if (Input.encPressed) {
        lastIndex = -1; 
        if (ui.settingsIndex == 0) { 
            ui.currentMode = MODE_SETTINGS_EDIT;
            ui.editId = 0;
            ui.prevVal = -1; 
            setLedState(true);
            UI::clear();
        } 
        else if (ui.settingsIndex == 1) { 
            ui.currentMode = MODE_SETTINGS_EDIT;
            ui.editId = 1;
            ui.prevVal = -1;
            UI::clear();
        }
        else if (ui.settingsIndex == 2) { 
            ui.currentMode = MODE_SETTINGS_EDIT;
            ui.editId = 2;
            ui.prevVal = -1;
            UI::clear();
        }
        else if (ui.settingsIndex == 3) { 
            ui.currentMode = MODE_WIFI_MENU; 
            ui.wifiMenuIndex = 0;
            ui.wifiMenuRedraw = true;
        }
    }
    if (Input.backPressed) {
        saveSettings();
        lastIndex = -1; 
        ui.currentMode = MODE_MENU;
    }
}

void runSettingsEdit() {
    bool valChanged = false;
    int currentVal = 0; 
    if (ui.editId == 0) { 
        if (Input.encStep != 0) {
            int newVal = constrain(settings.ledBrightness + (Input.encStep * 5), 0, 100);
            if (newVal != settings.ledBrightness) {
                settings.ledBrightness = newVal;
                setLedState(true); 
                valChanged = true;
            }
        }
        currentVal = settings.ledBrightness;
    } 
    else if (ui.editId == 1) { 
        if (Input.encStep != 0) {
            int newVal = constrain(settings.speakerVol + (Input.encStep * 5), 0, 100);
            if (newVal != settings.speakerVol) {
                settings.speakerVol = newVal;
                playSystemTone(2000, 20); 
                valChanged = true;
            }
        }
        currentVal = settings.speakerVol;
    }
    else if (ui.editId == 2) { 
        if (Input.encStep != 0) {
            int oldIdx = 0;
            for(int i=0; i<GRAPH_RANGES_COUNT; i++) {
                if (settings.graphDuration == GRAPH_RANGES_MIN[i]) { oldIdx = i; break; }
            }
            int newIdx = constrain(oldIdx + Input.encStep, 0, GRAPH_RANGES_COUNT - 1);
            if (newIdx != oldIdx) {
                settings.graphDuration = GRAPH_RANGES_MIN[newIdx];
                valChanged = true;
            }
        }
        currentVal = settings.graphDuration;
    }
    if (ui.prevVal == -1) {
        String title = (ui.editId==0) ? "LED Brightness" : (ui.editId==1) ? "Speaker Volume" : "Graph Range";
        UI::drawHeader(title);
        if (ui.editId != 2) {
             int barW = 260; int barH = 15; int barX = (Screen::WIDTH - barW)/2; int barY = 160;
             tft.drawRect(barX-1, barY-1, barW+2, barH+2, ST77XX_WHITE);
             tft.fillRect(barX, barY, barW, barH, Colors::DARK); 
             int fillW = (int)((float)barW * (currentVal / 100.0f));
             if(fillW > 0) tft.fillRect(barX, barY, fillW, barH, Colors::GREEN);
        }
        ui.prevVal = -999; 
        valChanged = true;
    }
    if (valChanged) {
        if (ui.editId == 2) {
            String oldBuf, newBuf;
            if (ui.prevVal != -999) {
                oldBuf = (ui.prevVal < 60) ? String(ui.prevVal) + "m" : String(ui.prevVal/60) + "h";
                UI::textCentered(oldBuf, 90, 5, Colors::BG);
            }
            newBuf = (currentVal < 60) ? String(currentVal) + "m" : String(currentVal/60) + "h";
            UI::textCentered(newBuf, 90, 5, ST77XX_WHITE);
        } 
        else { 
            char buf[16]; sprintf(buf, "%3d%%", currentVal); 
            UI::textCentered(String(buf), 90, 5, ST77XX_WHITE);
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
    if (Input.encPressed || Input.backPressed) {
        setLedState(false); 
        ui.currentMode = MODE_SETTINGS;
        ui.settingsIndex = ui.editId; 
    }
}

void runWiFiMenu() {
    static int lastWifiIndex = -1;
    bool selectionChanged = false;
    if (Input.encStep != 0) {
        ui.wifiMenuIndex += Input.encStep;
        if (ui.wifiMenuIndex < 0) ui.wifiMenuIndex = 1;
        if (ui.wifiMenuIndex > 1) ui.wifiMenuIndex = 0;
        selectionChanged = true;
    }
    if (ui.wifiMenuRedraw) {
        ui.wifiMenuRedraw = false;
        lastWifiIndex = -1; 
        selectionChanged = true; 
        UI::drawHeader("Current Network");
        String statusText = "Not Connected";
        uint16_t statusColor = ST77XX_RED;
        if (WiFi.status() == WL_CONNECTED) {
            statusText = WiFi.SSID();
            statusColor = Colors::GREEN;
        }
        UI::textCentered(statusText, 70, 3, statusColor);
    }
    if (selectionChanged || lastWifiIndex == -1) {
        const char* options[] = { "Setup", "Reset" };
        for (int i = 0; i < 2; i++) {
            int y = 140 + (i * 45);
            bool selected = (i == ui.wifiMenuIndex);
            uint16_t btnColor = (i == 1) ? ST77XX_RED : Colors::ACCENT;
            tft.fillRoundRect(40, y, 240, 35, 6, selected ? btnColor : Colors::BG);
            if (!selected) tft.drawRoundRect(40, y, 240, 35, 6, Colors::DARK);
            UI::textCenteredX(options[i], 40, 280, y + 10, 2, selected ? Colors::BG : ST77XX_WHITE);
        }
        lastWifiIndex = ui.wifiMenuIndex;
    }
    if (Input.encPressed) {
        if (ui.wifiMenuIndex == 0) {
            ui.currentMode = MODE_WIFI_SETUP;
            ui.wifiSetupInited = false;
        } else {
            ui.currentMode = MODE_WIFI_RESET_CONFIRM;
            ui.wifiResetConfirmIndex = 1; 
            ui.wifiResetRedraw = true;
        }
    }
    if (Input.backPressed) {
        ui.currentMode = MODE_SETTINGS;
        ui.settingsIndex = 3; 
    }
}

void runWiFiResetConfirm() {
    static int lastConfirmIndex = -1;
    bool selectionChanged = false;
    if (Input.encStep != 0) {
        ui.wifiResetConfirmIndex += Input.encStep;
        if (ui.wifiResetConfirmIndex < 0) ui.wifiResetConfirmIndex = 1;
        if (ui.wifiResetConfirmIndex > 1) ui.wifiResetConfirmIndex = 0;
        selectionChanged = true;
    }
    if (ui.wifiResetRedraw) {
        ui.wifiResetRedraw = false;
        lastConfirmIndex = -1;
        selectionChanged = true;
        UI::clear();
        UI::textCentered("WARNING!", 50, 3, ST77XX_RED);
        UI::textCentered("Reset WiFi Settings?", 90, 2, ST77XX_WHITE);
    }
    if (selectionChanged || lastConfirmIndex == -1) {
        const char* labels[] = { "YES", "NO" };
        int btnW = 100; int btnH = 40; int gap = 20;
        int startX = (Screen::WIDTH - (btnW * 2 + gap)) / 2;
        int y = 140;
        for (int i = 0; i < 2; i++) {
            int x = startX + i * (btnW + gap);
            bool selected = (i == ui.wifiResetConfirmIndex);
            uint16_t color = (i == 0) ? ST77XX_RED : Colors::GREEN; 
            
            tft.fillRoundRect(x, y, btnW, btnH, 6, selected ? color : Colors::BG);
            if (!selected) tft.drawRoundRect(x, y, btnW, btnH, 6, color);
            
            UI::textCenteredX(labels[i], x, x + btnW, y + (btnH - 16) / 2, 2, selected ? Colors::BG : color);
        }
        lastConfirmIndex = ui.wifiResetConfirmIndex;
    }
    if (Input.encPressed) {
        if (ui.wifiResetConfirmIndex == 0) {
            UI::clear();
            UI::text("Resetting...", 20, 100, 3, ST77XX_RED);
            playSystemTone(1000, 1000);
            wm.resetSettings();
            delay(500);
            ESP.restart();
        } else {
            ui.currentMode = MODE_WIFI_MENU;
            ui.wifiMenuRedraw = true; 
        }
    }
    if (Input.backPressed) {
        ui.currentMode = MODE_WIFI_MENU;
        ui.wifiMenuRedraw = true;
    }
}

void runWiFiSetup() {
    if (!ui.wifiSetupInited) {
        UI::clear();
        UI::textCentered("WiFi Setup", 30, 3, ST77XX_WHITE);
        UI::text("1. Connect to:", 20, 70, 2, ST77XX_WHITE);
        UI::text("CyberClockSetup", 56, 95, 2, Colors::GREEN);
        UI::text("2. Go to IP:", 20, 130, 2, ST77XX_WHITE);
        UI::text("192.168.4.1", 56, 155, 2, Colors::GREEN);
        wm.setConfigPortalBlocking(false);
        wm.startConfigPortal("CyberClockSetup");
        ui.wifiSetupInited = true;
    }
    wm.process();
    if (Input.backPressed) {
        wm.stopConfigPortal();
        ui.currentMode = MODE_WIFI_MENU; 
        ui.wifiMenuRedraw = true;        
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        UI::clear();
        UI::textCentered("Success!", 100, 3, Colors::GREEN);
        UI::textCentered("WiFi Connected", 140, 2, ST77XX_WHITE);
        delay(1500); 
        syncTime();
        wm.stopConfigPortal();
        ui.currentMode = MODE_SETTINGS;
        ui.settingsIndex = 3; 
        ui.settingsInited = false; 
    }
}

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
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, NULL);
    }
}