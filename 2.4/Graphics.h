#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "Config.h"
#include "Globals.h"

namespace UI {
void clear(uint16_t bgColor = Colors::BG);
void drawHeader(String title);
void text(String str, int x, int y, int size, uint16_t color,
          uint16_t bg = Colors::BG);
void textCentered(String str, int y, int size, uint16_t color,
                  uint16_t bg = Colors::BG);
void textCenteredX(String str, int x_start, int x_end, int y, int size,
                   uint16_t color, uint16_t bg = Colors::BG);
void drawBar(int x, int y, int w, int h, int percent, uint16_t color,
             int &prevW);
void drawListItem(int index, int selectedIndex, const char *label,
                  bool fullRedraw);
} // namespace UI

void drawAlarmIcon();
void drawHistoryGraph();
void initClockStaticUI();
void drawClockTime(String hourStr, String minStr, String secStr);
void drawEnvDynamic();
void drawGenericList(const char *items[], int count, int selectedIndex,
                     int lastIndex, bool fullRedraw);
void drawPomodoroScreen(bool forceStatic);
void updatePomodoroValue(int value);
void drawAlarmRingingScreen();
void drawAlarmScreen(bool full);
void drawDvdLogo(int x, int y, uint16_t c);
void initDvd();
void drawSettingsScreen(bool full);

#endif