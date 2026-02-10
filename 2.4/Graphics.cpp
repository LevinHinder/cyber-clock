#include "Graphics.h"

namespace UI {
    void clear(uint16_t bgColor) {
        tft.fillScreen(bgColor);
        if(bgColor == Colors::BG) drawAlarmIcon();
    }

    void drawHeader(String title) {
        clear();
        textCentered(title, 40, 2, Colors::LIGHT);
    }

    void text(String str, int x, int y, int size, uint16_t color, uint16_t bg) {
        tft.setTextSize(size);
        tft.setTextColor(color, bg);
        tft.setCursor(x, y);
        tft.print(str);
    }

    void textCentered(String str, int y, int size, uint16_t color, uint16_t bg) {
        int16_t x1, y1; uint16_t w, h;
        tft.setTextSize(size);
        tft.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor((Screen::WIDTH - w) / 2, y);
        tft.setTextColor(color, bg);
        tft.print(str);
    }

    void textCenteredX(String str, int x_start, int x_end, int y, int size, uint16_t color, uint16_t bg) {
        int16_t x1, y1; uint16_t w, h;
        tft.setTextSize(size);
        tft.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
        int x = x_start + ((x_end - x_start) - (int)w) / 2;
        text(str, x, y, size, color, bg);
    }

    void drawBar(int x, int y, int w, int h, int percent, uint16_t color, int &prevW) {
        if (prevW == -1) {
            tft.drawRect(x - 1, y - 1, w + 2, h + 2, ST77XX_WHITE);
            tft.fillRect(x, y, w, h, Colors::DARK);
            prevW = 0;
        }
        int targetW = (int)((float)w * (percent / 100.0f));
        if (targetW != prevW) {
            if (targetW > prevW) tft.fillRect(x + prevW, y, targetW - prevW, h, color);
            else tft.fillRect(x + targetW, y, prevW - targetW, h, Colors::DARK);
            prevW = targetW;
        }
    }

    void drawListItem(int index, int selectedIndex, const char* label, bool fullRedraw) {
         int rowCenterY = 60 + index * 35; 
         int boxY = rowCenterY - 14; int boxH = 28; int boxW = 300; int boxX = 10;
         int textY = rowCenterY - 7; int textX = 24;
         
         bool selected = (index == selectedIndex);
         if (selected) {
             tft.fillRect(boxX, boxY, boxW, boxH, Colors::ACCENT);
             text(label, textX, textY, 2, Colors::BG, Colors::ACCENT);
         } else {
             tft.fillRect(boxX, boxY, boxW, boxH, Colors::BG);
             text(label, textX, textY, 2, ST77XX_WHITE, Colors::BG);
         }
    }
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
        int x = i;
        // Simple scaling logic kept same
        int yT = map(constrain(env.hTemp[idx]/10, 0, 50), 0, 50, 88, 2);
        int yH = map(constrain(env.hHum[idx], 0, 100), 0, 100, 88, 2);
        int yV = map(constrain(env.hTVOC[idx], 0, 1500), 0, 1500, 88, 2);
        int yC = map(constrain(env.hCO2[idx], 0, 2000), 0, 2000, 88, 2);
        
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

void initClockStaticUI() {
    UI::clear();
    tft.drawFastHLine(Layout::GRID_L, Layout::GRID_TOP, Layout::GRID_R - Layout::GRID_L, ST77XX_WHITE);
    tft.drawFastHLine(Layout::GRID_L, Layout::GRID_MID, Layout::GRID_R - Layout::GRID_L, ST77XX_WHITE);
    tft.drawFastHLine(Layout::GRID_L, Layout::GRID_BOT, Layout::GRID_R - Layout::GRID_L, ST77XX_WHITE);
    tft.drawFastVLine(Layout::GRID_MID_X, Layout::GRID_TOP, Layout::GRID_BOT - Layout::GRID_TOP, ST77XX_WHITE);
    UI::textCenteredX("HUMI", Layout::GRID_L, Layout::GRID_MID_X, Layout::LBL_TOP_Y, 1, ST77XX_WHITE);
    UI::textCenteredX("TEMP", Layout::GRID_MID_X, Layout::GRID_R, Layout::LBL_TOP_Y, 1, ST77XX_WHITE);
    UI::textCenteredX("TVOC", Layout::GRID_L, Layout::GRID_MID_X, Layout::LBL_BOT_Y, 1, ST77XX_WHITE);
    UI::textCenteredX("CO2",  Layout::GRID_MID_X, Layout::GRID_R, Layout::LBL_BOT_Y, 1, ST77XX_WHITE);
    drawHistoryGraph();
}

void drawClockTime(String hourStr, String minStr, String secStr) {
    String cur = hourStr + ":" + minStr + ":" + secStr;
    if (cur == ui.prevTimeStr) return;
    ui.prevTimeStr = cur;
    UI::textCentered(cur, 10, 6, ST77XX_WHITE);
}

void drawEnvDynamic() {
    char buf[32];

    sprintf(buf, " %2.0f%% ", env.hum);
    UI::textCenteredX(String(buf), Layout::GRID_L, Layout::GRID_MID_X, Layout::VAL_TOP_Y, 2, Colors::HUM, Colors::BG);
    sprintf(buf, " %2.1fC ", env.temp);
    UI::textCenteredX(String(buf), Layout::GRID_MID_X, Layout::GRID_R, Layout::VAL_TOP_Y, 2, Colors::TEMP, Colors::BG);
    sprintf(buf, "  %d  ", env.tvoc);
    UI::textCenteredX(String(buf), Layout::GRID_L, Layout::GRID_MID_X, Layout::VAL_BOT_Y, 2, Colors::TVOC, Colors::BG);
    sprintf(buf, "  %d  ", env.eco2);
    UI::textCenteredX(String(buf), Layout::GRID_MID_X, Layout::GRID_R, Layout::VAL_BOT_Y, 2, Colors::CO2, Colors::BG);
}

void drawGenericList(const char* items[], int count, int selectedIndex, int lastIndex, bool fullRedraw) {
    if (fullRedraw) UI::clear();
    for (int i = 0; i < count; i++) {
        if (fullRedraw || i == selectedIndex || i == lastIndex) {
            UI::drawListItem(i, selectedIndex, items[i], fullRedraw);
        }
    }
}

void drawPomodoroScreen(bool forceStatic) {
    if (forceStatic) {
        UI::clear();
        pomo.prevBarWidth = -1; 
        pomo.prevLabel = "";
        ui.prevTimeStr = ""; 
        char cycleBuf[20]; sprintf(cycleBuf, "Cycle: %d/%d", pomo.currentCycle, settings.pomoCycles);
        UI::textCentered(cycleBuf, 190, 2, ST77XX_WHITE);
    }
    
    String labelStr = "";
    uint16_t labelColor = Colors::LIGHT;
    if (pomo.state == POMO_PAUSED) { labelStr = "Paused"; labelColor = ST77XX_YELLOW; } 
    else if (pomo.phase == PHASE_WORK) { labelStr = "Get to Work!"; labelColor = Colors::LIGHT; } 
    else if (pomo.phase == PHASE_SHORT) { labelStr = "Short Break"; labelColor = Colors::GREEN; } 
    else { labelStr = "Long Break"; labelColor = Colors::BLUE; }

    if (labelStr != pomo.prevLabel) {
        tft.fillRect(0, 30, Screen::WIDTH, 20, Colors::BG); 
        UI::textCentered(labelStr, 30, 2, labelColor);
        pomo.prevLabel = labelStr;
    }

    unsigned long elapsed = 0;
    if (pomo.state == POMO_RUNNING) elapsed = millis() - pomo.startMillis;
    else if (pomo.state == POMO_PAUSED) elapsed = pomo.pausedMillis - pomo.startMillis;
    if (elapsed > pomo.durationMs) elapsed = pomo.durationMs;
    float progress = (pomo.durationMs > 0) ? (float)elapsed / pomo.durationMs : 0.0f;
    
    UI::drawBar(20, 225, 280, 10, (int)(progress * 100), Colors::GREEN, pomo.prevBarWidth);

    unsigned long remain = pomo.durationMs - elapsed;
    char buf[8]; sprintf(buf, "%02d:%02d", (int)(remain / 60000UL), (int)((remain / 1000UL) % 60));
    String timeStr = String(buf);
    static uint16_t prevTimeColor = 0;
    uint16_t timeColor = (pomo.state == POMO_PAUSED) ? Colors::LIGHT : ST77XX_WHITE;
    if (timeStr != ui.prevTimeStr || timeColor != prevTimeColor) {
         UI::textCentered(timeStr, 90, 6, timeColor);
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
    UI::textCentered(String(value), numY, 6, ST77XX_WHITE);
}

void drawAlarmRingingScreen() {
    UI::clear(ST77XX_RED);
    UI::text("ALARM!", 60, 100, 4, ST77XX_WHITE, ST77XX_RED);
}

void drawAlarmScreen(bool full) {
    if (full) {
        UI::clear();
        UI::text("Status:", 50, 165, 3, ST77XX_WHITE);
    }
    char hBuf[3], mBuf[3];
    sprintf(hBuf, "%02d", settings.alarmHour);
    sprintf(mBuf, "%02d", settings.alarmMinute);
    
    int16_t x1, y1; uint16_t w, h;
    tft.setTextSize(6);
    tft.getTextBounds("88:88", 0, 0, &x1, &y1, &w, &h);
    int x = (Screen::WIDTH - w) / 2;
    
    tft.setCursor(x, 60);
    tft.setTextColor(ui.alarmSelectedField == 0 ? Colors::LIGHT : ST77XX_WHITE, Colors::BG);
    tft.print(hBuf);
    tft.setTextColor(ST77XX_WHITE, Colors::BG);
    tft.print(":");
    tft.setTextColor(ui.alarmSelectedField == 1 ? Colors::LIGHT : ST77XX_WHITE, Colors::BG);
    tft.print(mBuf);

    uint16_t enColor = ui.alarmSelectedField == 2 ? Colors::LIGHT : (settings.alarmEnabled ? Colors::GREEN : ST77XX_RED);
    UI::text(settings.alarmEnabled ? "ON " : "OFF", 180, 165, 3, enColor);
}

void initDvd() {
    ui.dvdInited = true;
    UI::clear();
    ui.dvdX = 80; ui.dvdY = 80; ui.dvdVX = 3; ui.dvdVY = 2;
    ui.lastDvdMs = millis();
    ui.dvdColorIndex = 0;
}

void drawDvdLogo(int x, int y, uint16_t c) {
    tft.fillRoundRect(x, y, ui.dvdW, ui.dvdH, 8, c);
    tft.drawRoundRect(x, y, ui.dvdW, ui.dvdH, 8, Colors::BG);
    UI::textCenteredX("DVD", x, x + ui.dvdW, y + ui.dvdH/2 - 6, 2, Colors::BG, c);
}

void drawSettingsScreen(bool full) {
    if (full) {
        UI::clear();
        ui.prevLedBarW = -1;
        ui.prevSpkBarW = -1;
    }
    int barX = 40; int barW = 240; int barH = 15;
    uint16_t cLed = (ui.settingsSelectedRow == 0) ? (ui.settingsEditMode ? Colors::GREEN : Colors::BLUE) : ST77XX_WHITE;
    uint16_t cSpk = (ui.settingsSelectedRow == 1) ? (ui.settingsEditMode ? Colors::GREEN : Colors::BLUE) : ST77XX_WHITE;
    uint16_t cGr  = (ui.settingsSelectedRow == 2) ? (ui.settingsEditMode ? Colors::GREEN : Colors::BLUE) : ST77XX_WHITE;
    
    UI::text("LED Brightness", barX, 20, 2, cLed);
    UI::drawBar(barX, 40, barW, barH, settings.ledBrightness, Colors::GREEN, ui.prevLedBarW);

    UI::text("Speaker Volume", barX, 60, 2, cSpk);
    UI::drawBar(barX, 80, barW, barH, settings.speakerVol, Colors::GREEN, ui.prevSpkBarW);

    UI::text("Graph Range", barX, 100, 2, cGr);
    tft.drawRect(barX - 1, 119, barW + 2, barH + 12, ST77XX_WHITE); 
    tft.fillRect(barX, 120, barW, barH + 10, Colors::DARK);
    
    String rangeStr = (settings.graphDuration < 60) ? String(settings.graphDuration) + " Min" : String(settings.graphDuration / 60) + " Hours";
    UI::textCenteredX(rangeStr, barX, barX + barW, 124, 2, ST77XX_WHITE, Colors::DARK);

    // WiFi Buttons
    int yWifi = 165;
    tft.fillRoundRect(barX, yWifi, barW, 25, 4, (ui.settingsSelectedRow == 3) ? Colors::BLUE : Colors::DARK);
    UI::textCenteredX("Setup WiFi", barX, barX + barW, yWifi + 5, 2, (ui.settingsSelectedRow == 3) ? Colors::BG : ST77XX_WHITE, (ui.settingsSelectedRow == 3) ? Colors::BLUE : Colors::DARK);

    int yReset = 200;
    tft.fillRoundRect(barX, yReset, barW, 25, 4, (ui.settingsSelectedRow == 4) ? ST77XX_RED : Colors::DARK);
    UI::textCenteredX("Reset WiFi", barX, barX + barW, yReset + 5, 2, (ui.settingsSelectedRow == 4) ? Colors::BG : ST77XX_WHITE, (ui.settingsSelectedRow == 4) ? ST77XX_RED : Colors::DARK);
}