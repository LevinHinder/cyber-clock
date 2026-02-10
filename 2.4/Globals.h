#ifndef GLOBALS_H
#define GLOBALS_H

#include "Config.h"
#include "InputManager.h"
#include "Types.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>
#include <ScioSense_ENS160.h>
#include <WiFiManager.h>

extern Adafruit_ST7789 tft;
extern GFXcanvas16 graphCanvas;
extern Adafruit_AHTX0 aht;
extern ScioSense_ENS160 ens160;
extern Preferences prefs;
extern WiFiManager wm;

extern AppSettings settings;
extern EnvData env;
extern PomodoroContext pomo;
extern UIContext ui;
extern InputManager Input;

extern const int GRAPH_RANGES_MIN[];
extern const int GRAPH_RANGES_COUNT;
extern uint16_t dvdPalette[];

#endif