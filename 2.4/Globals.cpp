#include "Globals.h"

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
InputManager Input;

const int GRAPH_RANGES_MIN[] = { 5, 15, 30, 60, 180, 360, 720, 1440 };
const int GRAPH_RANGES_COUNT = 8;
uint16_t dvdPalette[] = { ST77XX_WHITE, Colors::ACCENT, Colors::LIGHT, Colors::GREEN, Colors::PINK, ST77XX_YELLOW };