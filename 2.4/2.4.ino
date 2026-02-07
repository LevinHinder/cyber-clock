#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include "time.h"

// ====== WiFi & Time config ======
const char* ssid      = "Hinder WLAN";
const char* password  = "t&5yblzK6*e#";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec       = 7 * 3600;   // GMT+7
const int   daylightOffset_sec = 0;

// ====== Pins ======
#define TFT_CS   9
#define TFT_DC   8
#define TFT_RST  7
#define TFT_MOSI 6
#define TFT_SCLK 5

#define ENC_A_PIN    10
#define ENC_B_PIN    20
#define ENC_BTN_PIN  21
#define KEY0_PIN     0

#define SDA_PIN 1
#define SCL_PIN 2

#define LED_PIN 4
#define BUZZ_PIN 3

// ====== TFT & Sensors ======
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); 
Adafruit_AHTX0   aht;
ScioSense_ENS160 ens160(0x53);

// ====== Screen Dimensions ======
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_CX     (SCREEN_WIDTH / 2)
#define SCREEN_CY     (SCREEN_HEIGHT / 2)

// ====== Colors ======
#define CYBER_BG      ST77XX_BLACK
#define CYBER_GREEN   0x07E0  
#define CYBER_ACCENT  0x07FF  
#define CYBER_LIGHT   0xFD20  
#define CYBER_BLUE    0x07FF  
#define CYBER_PINK    0xF81F

#define AQ_BAR_GREEN  0x07E0
#define AQ_BAR_YELLOW 0xFFE0
#define AQ_BAR_ORANGE 0xFD20
#define AQ_BAR_RED    0xF800
#define CYBER_DARK    0x4208
#define GRID_COLOR    0x2104 

#ifndef PI
#define PI 3.1415926
#endif

// ====== Modes ======
enum UIMode {
  MODE_MENU = 0,
  MODE_CLOCK,
  MODE_POMODORO,
  MODE_ALARM,
  MODE_DVD
};
UIMode currentMode = MODE_CLOCK;       
int menuIndex = 0;
const int MENU_ITEMS = 4;

// ====== Pomodoro ======
enum PomodoroState {
  POMO_SELECT = 0,
  POMO_RUNNING,
  POMO_PAUSED,
  POMO_DONE
};
PomodoroState pomoState = POMO_SELECT;
int  pomoPresetIndex    = 0;
const uint16_t pomoDurationsMin[3] = {5, 15, 30};
unsigned long pomoStartMillis = 0;
unsigned long pomoPausedMillis = 0;
int prevPomoRemainSec  = -1;
int prevPomoPreset     = -1;
int prevPomoStateInt   = -1;

// ====== Env values ======
float    curTemp = 0;
float    curHum  = 0;
uint16_t curTVOC = 0;
uint16_t curECO2 = 400;
unsigned long lastEnvRead = 0;

// ====== Graph History ======
#define HISTORY_LEN 320 
uint16_t histTemp[HISTORY_LEN];
uint16_t histHum[HISTORY_LEN];
uint16_t histTVOC[HISTORY_LEN];
uint16_t histCO2[HISTORY_LEN];

int historyHead = 0;
unsigned long lastHistoryAdd = 0;
const unsigned long HISTORY_INTERVAL = 60000; 

// Forward declarations
void drawHistoryGraph(); 
void printCenteredText(const String &txt, int x0, int x1, int y, uint16_t color, uint16_t bg, uint8_t size);

// ====== Clock vars ======
int    prevSecond  = -1;
String prevTimeStr = "";

// ====== Encoder & Buttons ======
int  lastEncA   = HIGH;
int  lastEncB   = HIGH;
bool lastEncBtn = HIGH;
bool lastKey0   = HIGH;
unsigned long lastBtnMs = 0;

// ====== Alarm ======
bool     alarmEnabled = false;
uint8_t  alarmHour    = 7;
uint8_t  alarmMinute  = 0;
bool     alarmRinging = false;
int      alarmSelectedField = 0; 
int      lastAlarmDayTriggered = -1;

// ====== Alert / LED Blink ======
enum AlertLevel {
  ALERT_NONE = 0,
  ALERT_CO2,
  ALERT_ALARM
};
AlertLevel currentAlertLevel = ALERT_NONE;
unsigned long lastLedToggleMs = 0;
bool ledState = false;

unsigned long lastCo2BlinkMs = 0;
bool co2BlinkOn = false;

// ========= Layout Constants (MOVED DOWN) =========
const int GRID_L   = 10;
const int GRID_R   = 310;
const int GRID_MID_X = (GRID_L + GRID_R) / 2;

// Moved Top down to 75 (was 60)
// Height remains 32px per row
const int GRID_TOP = 75; 
const int GRID_MID = 107;
const int GRID_BOT = 139;

// Offsets
const int TOP_LABEL_Y    = GRID_TOP + 4;  
const int TOP_VALUE_Y    = GRID_TOP + 14; 
const int BOTTOM_LABEL_Y = GRID_MID + 4;  
const int BOTTOM_VALUE_Y = GRID_MID + 14; 

// ========= Helper: encoder & button =========
int readEncoderStep() {
  int encA = digitalRead(ENC_A_PIN);
  int encB = digitalRead(ENC_B_PIN);
  int step = 0;
  if (encA != lastEncA) {
    if (encA == LOW) {
      if (encB == HIGH) step = +1;
      else              step = -1;
    }
  }
  lastEncA = encA;
  lastEncB = encB;
  return step;
}

bool checkButtonPressed(uint8_t pin, bool &lastState) {
  bool cur = digitalRead(pin);
  bool pressed = false;
  unsigned long now = millis();
  if (cur == LOW && lastState == HIGH && (now - lastBtnMs) > 150) {
    pressed = true;
    lastBtnMs = now;
  }
  lastState = cur;
  return pressed;
}

// ========= UI Helpers =========
void drawAlarmIcon() {
  int x = SCREEN_WIDTH - 24; 
  tft.fillRect(x - 10, 0, 24, 24, CYBER_BG);
  if (!alarmEnabled) return;

  uint16_t c = CYBER_LIGHT; 
  tft.drawRoundRect(x - 9, 4, 18, 14, 4, c);
  tft.drawFastHLine(x - 7, 16, 14, c);
  tft.fillCircle(x, 19, 2, c);
}

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

uint16_t colorForCO2(uint16_t eco2) {
  if (eco2 <= 800)  return AQ_BAR_GREEN;
  if (eco2 <= 1200) return AQ_BAR_YELLOW;
  if (eco2 <= 1800) return AQ_BAR_ORANGE;
  return AQ_BAR_RED;
}

// ========= WiFi & Time =========
void connectWiFiAndSyncTime() {
  tft.fillScreen(CYBER_BG);
  tft.setTextColor(CYBER_LIGHT);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.print("Connecting WiFi");
  WiFi.begin(ssid, password);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(300);
    tft.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    tft.fillScreen(CYBER_BG);
    tft.setCursor(20, 100);
    tft.print("Syncing time...");
    delay(800);
  } else {
    tft.fillScreen(CYBER_BG);
    tft.setCursor(20, 100);
    tft.setTextColor(ST77XX_RED);
    tft.print("WiFi FAILED!");
    delay(1000);
  }
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

// ========= Env Sensors & History Logic =========
void recordHistory(float t, float h, uint16_t tvoc, uint16_t eco2) {
  histTemp[historyHead] = (uint16_t)(t * 10); 
  histHum[historyHead]  = (uint16_t)h;
  histTVOC[historyHead] = tvoc;
  histCO2[historyHead]  = eco2;
  
  historyHead = (historyHead + 1) % HISTORY_LEN;
  
  if (currentMode == MODE_CLOCK) {
    drawHistoryGraph();
  }
}

void updateEnvSensors(bool force = false) {
  unsigned long now = millis();
  
  if (force || (now - lastEnvRead) > 5000) {
    lastEnvRead = now;
    sensors_event_t hum, temp;
    if (aht.getEvent(&hum, &temp)) {
      curTemp = temp.temperature;
      curHum  = hum.relative_humidity;
    }
    ens160.set_envdata(curTemp, curHum);
    ens160.measure();
    uint16_t newTVOC = ens160.getTVOC();
    uint16_t newCO2  = ens160.geteCO2();
    if (newTVOC != 0xFFFF) curTVOC = newTVOC;
    if (newCO2  != 0xFFFF) curECO2 = newCO2;
  }

  if (now - lastHistoryAdd > HISTORY_INTERVAL) {
    lastHistoryAdd = now;
    recordHistory(curTemp, curHum, curTVOC, curECO2);
  }
}

// ========= Graph Drawing (4 Lines) =========
void drawHistoryGraph() {
  const int gX = 0;
  const int gY = 145;      // Moved down to accommodate table (ends at 139)
  const int gW = 320;
  const int gH = 90;       // Reduced height to fit screen bottom
  const int gBottom = gY + gH; // Ends at 235
  
  // Clear Graph Area
  tft.fillRect(gX, gY, gW, gH, CYBER_BG);
  
  // Grid
  tft.drawFastHLine(gX, gY + gH/4, gW, GRID_COLOR);
  tft.drawFastHLine(gX, gY + gH/2, gW, GRID_COLOR);
  tft.drawFastHLine(gX, gY + 3*gH/4, gW, GRID_COLOR);

  int pX = -1;
  int pY_T = -1, pY_H = -1, pY_V = -1, pY_C = -1;

  for (int i = 0; i < HISTORY_LEN; i++) {
    int idx = (historyHead + i) % HISTORY_LEN;
    
    // Scale Values to Graph Height
    // Temp: 0 - 50 C
    int valT = histTemp[idx] / 10; 
    int yT = map(constrain(valT, 0, 50), 0, 50, gBottom-2, gY+2);

    // Hum: 0 - 100 %
    int valH = histHum[idx];
    int yH = map(constrain(valH, 0, 100), 0, 100, gBottom-2, gY+2);

    // TVOC: 0 - 1000
    int valV = histTVOC[idx];
    int yV = map(constrain(valV, 0, 1000), 0, 1000, gBottom-2, gY+2);

    // CO2: 400 - 3000
    int valC = histCO2[idx];
    int yC = map(constrain(valC, 400, 3000), 400, 3000, gBottom-2, gY+2);

    int x = i;

    if (i > 0) {
       tft.drawLine(pX, pY_T, x, yT, CYBER_LIGHT); // Orange (Temp)
       tft.drawLine(pX, pY_H, x, yH, CYBER_BLUE);  // Blue (Hum)
       tft.drawLine(pX, pY_V, x, yV, CYBER_GREEN); // Green (TVOC)
       tft.drawLine(pX, pY_C, x, yC, ST77XX_WHITE); // White (CO2)
    }

    pX = x;
    pY_T = yT;
    pY_H = yH;
    pY_V = yV;
    pY_C = yC;
  }

  // Border
  tft.drawRect(gX, gY, gW, gH, GRID_COLOR);
}

// ========= Clock UI =========
void initClockStaticUI() {
  tft.fillScreen(CYBER_BG);
  drawAlarmIcon();
  
  // Draw Grid Lines (Moved Down)
  tft.drawFastHLine(GRID_L, GRID_TOP, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastHLine(GRID_L, GRID_MID, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastHLine(GRID_L, GRID_BOT, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastVLine(GRID_MID_X, GRID_TOP, GRID_BOT - GRID_TOP, ST77XX_WHITE);

  // Draw Static Labels
  printCenteredText("HUMI", GRID_L,     GRID_MID_X, TOP_LABEL_Y,    ST77XX_WHITE, CYBER_BG, 1);
  printCenteredText("TEMP", GRID_MID_X, GRID_R,     TOP_LABEL_Y,    ST77XX_WHITE, CYBER_BG, 1);
  printCenteredText("TVOC", GRID_L,     GRID_MID_X, BOTTOM_LABEL_Y, ST77XX_WHITE, CYBER_BG, 1);
  printCenteredText("CO2",  GRID_MID_X, GRID_R,     BOTTOM_LABEL_Y, ST77XX_WHITE, CYBER_BG, 1);

  drawHistoryGraph();
}

void drawClockTime(String hourStr, String minStr, String secStr) {
  String cur = hourStr + ":" + minStr + ":" + secStr;
  if (cur == prevTimeStr) return;
  prevTimeStr = cur;

  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(6); 
  
  tft.getTextBounds("88:88:88", 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = 10; 

  tft.setTextColor(CYBER_ACCENT, CYBER_BG);
  tft.setCursor(x, y);
  tft.print(cur);
}

void drawEnvDynamic(float temp, float hum, uint16_t tvoc, uint16_t eco2) {
  uint16_t colHUMI = CYBER_BLUE;
  uint16_t colTEMP = CYBER_LIGHT;
  uint16_t colTVOC = CYBER_GREEN;
  uint16_t colCO2  = colorForCO2(eco2);

  // HUMI
  char humBuf[8];
  sprintf(humBuf, "%2.0f%%", hum);
  printCenteredText(String(humBuf),
                    GRID_L, GRID_MID_X,
                    TOP_VALUE_Y,
                    colHUMI, CYBER_BG, 2);

  // TEMP
  char tempBuf[10];
  sprintf(tempBuf, "%2.1fC", temp);
  printCenteredText(String(tempBuf),
                    GRID_MID_X, GRID_R,
                    TOP_VALUE_Y,
                    colTEMP, CYBER_BG, 2);

  // TVOC
  char tvocBuf[16];
  sprintf(tvocBuf, "%d", tvoc);
  printCenteredText(String(tvocBuf),
                    GRID_L, GRID_MID_X,
                    BOTTOM_VALUE_Y,
                    colTVOC, CYBER_BG, 2);

  // CO2
  char co2Buf[12];
  sprintf(co2Buf, "%d", eco2);
  printCenteredText(String(co2Buf),
                    GRID_MID_X, GRID_R,
                    BOTTOM_VALUE_Y,
                    colCO2, CYBER_BG, 2);
}

// ========= Menu UI =========
void drawMenu() {
  tft.fillScreen(CYBER_BG);
  tft.setTextSize(2); 
  
  const char* items[MENU_ITEMS] = {
    "Monitor",
    "Pomodoro",
    "Alarm",
    "DVD"
  };

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 40 + i * 40; 
    if (i == menuIndex) {
      tft.fillRect(10, y - 4, 300, 28, CYBER_ACCENT);
      tft.setTextColor(CYBER_BG);
    } else {
      tft.fillRect(10, y - 4, 300, 28, CYBER_BG);
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(24, y);
    tft.print(items[i]);
  }
  drawAlarmIcon();
}

// ========= Pomodoro =========
uint16_t pomoColorFromFrac(float f) {
  if (f < 0.33f) return AQ_BAR_GREEN;
  if (f < 0.66f) return AQ_BAR_YELLOW;
  if (f < 0.85f) return AQ_BAR_ORANGE;
  return AQ_BAR_RED;
}

void drawPomodoroRing(float progress) {
  int cx = SCREEN_CX; 
  int cy = SCREEN_CY; 
  int rOuter = 100;   
  int rInner = 80;    

  const float startDeg = -225.0f;
  const float endDeg   =   45.0f;
  const float spanDeg  = endDeg - startDeg;   

  for (float deg = startDeg; deg <= endDeg; deg += 3.0f) { 
    float frac = (deg - startDeg) / spanDeg; 
    uint16_t col = (frac <= progress) ? pomoColorFromFrac(frac) : CYBER_DARK;

    float rad = deg * PI / 180.0f;
    for(int i=0; i<3; i++) {
        int xOuter = cx + cos(rad) * (rOuter+i);
        int yOuter = cy + sin(rad) * (rOuter+i);
        int xInner = cx + cos(rad) * (rInner-i);
        int yInner = cy + sin(rad) * (rInner-i);
        tft.drawLine(xInner, yInner, xOuter, yOuter, col);
    }
  }
}

void drawPomodoroScreen(bool forceStatic) {
  bool needStatic = forceStatic;
  if (prevPomoPreset != pomoPresetIndex || prevPomoStateInt != (int)pomoState) {
    needStatic = true;
    prevPomoPreset   = pomoPresetIndex;
    prevPomoStateInt = (int)pomoState;
  }

  if (needStatic) {
    tft.fillScreen(CYBER_BG);
    drawAlarmIcon();
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.fillRect(200, 0, 100, 30, CYBER_BG);
  tft.setCursor(210, 8);
  tft.printf("%2d min", pomoDurationsMin[pomoPresetIndex]);

  unsigned long durationMs = pomoDurationsMin[pomoPresetIndex] * 60UL * 1000UL;
  unsigned long elapsed = 0;
  if (pomoState == POMO_RUNNING)      elapsed = millis() - pomoStartMillis;
  else if (pomoState == POMO_PAUSED)  elapsed = pomoPausedMillis - pomoStartMillis;
  else if (pomoState == POMO_DONE)    elapsed = durationMs;
  if (elapsed > durationMs) elapsed = durationMs;

  float progress = (durationMs > 0) ? (float)elapsed / durationMs : 0.0f;
  drawPomodoroRing(progress);

  int cx = SCREEN_CX;
  int cy = SCREEN_CY;
  tft.fillCircle(cx, cy, 70, CYBER_BG); 

  unsigned long remain = durationMs - elapsed;
  uint16_t rmMin = remain / 60000UL;
  uint8_t  rmSec = (remain / 1000UL) % 60;

  char buf[8];
  sprintf(buf, "%02d:%02d", rmMin, rmSec);

  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(5); 
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - 15);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.print(buf);

  tft.fillRect(0, 200, SCREEN_WIDTH, 40, CYBER_BG);
  tft.setTextSize(2);
  tft.setCursor(16, 205);
  tft.setTextColor(CYBER_GREEN, CYBER_BG);
  if (pomoState == POMO_PAUSED)      tft.print("Paused");
  else if (pomoState == POMO_DONE)   tft.print("Completed");
}

// ========= Alarm UI (NO TITLE) =========
void drawAlarmScreen(bool full) {
  if (full) {
    tft.fillScreen(CYBER_BG);
    drawAlarmIcon();
  }

  char hBuf[3];
  char mBuf[3];
  sprintf(hBuf, "%02d", alarmHour);
  sprintf(mBuf, "%02d", alarmMinute);

  tft.setTextSize(6);

  String disp = String(hBuf) + ":" + String(mBuf);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(disp, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = 60;

  tft.setCursor(x, y);
  tft.setTextColor(alarmSelectedField == 0 ? CYBER_LIGHT : ST77XX_WHITE, CYBER_BG);
  tft.print(hBuf);

  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.print(":");

  tft.setTextColor(alarmSelectedField == 1 ? CYBER_LIGHT : ST77XX_WHITE, CYBER_BG);
  tft.print(mBuf);

  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.fillRect(40, 160, 240, 40, CYBER_BG);
  tft.setCursor(50, 165);
  tft.print("Status:");
  tft.setCursor(180, 165);
  uint16_t enColor = alarmSelectedField == 2
                     ? CYBER_LIGHT
                     : (alarmEnabled ? CYBER_GREEN : ST77XX_RED);
  tft.setTextColor(enColor, CYBER_BG);
  tft.print(alarmEnabled ? "ON " : "OFF");
}

void drawAlarmRingingScreen() {
  tft.fillScreen(ST77XX_RED);
  tft.setTextSize(4);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(60, 100);
  tft.print("ALARM!");
}

// ========= Alarm logic =========
void checkAlarmTrigger() {
  if (!alarmEnabled || alarmRinging) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == alarmHour &&
      timeinfo.tm_min  == alarmMinute &&
      timeinfo.tm_sec  == 0) {

    alarmRinging = true;
    lastAlarmDayTriggered = timeinfo.tm_mday;
    currentMode = MODE_ALARM;
    drawAlarmRingingScreen();
  }
}

// ========= Alert visual + audio =========
void updateAlertStateAndLED() {
  if (alarmRinging) currentAlertLevel = ALERT_ALARM;
  else if (curECO2 > 1800) currentAlertLevel = ALERT_CO2;
  else currentAlertLevel = ALERT_NONE;

  unsigned long now = millis();

  if (currentAlertLevel == ALERT_NONE) {
    digitalWrite(LED_PIN, LOW);
    ledState = false; 
  } else {
    unsigned long interval;
    if (currentAlertLevel == ALERT_ALARM)      interval = 120; 
    else /* ALERT_CO2 */                       interval = 250; 

    if (now - lastLedToggleMs > interval) {
      lastLedToggleMs = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }

  if (currentAlertLevel == ALERT_CO2) {
    if (now - lastCo2BlinkMs > 350) {
      lastCo2BlinkMs = now;
      co2BlinkOn = !co2BlinkOn;
      tone(BUZZ_PIN, 1800, 80);
    }
  }
}

// ========= DVD screensaver (NO TITLE) =========
bool dvdInited = false;
int  dvdX, dvdY;
int  dvdVX = 2;
int  dvdVY = 1;
int  dvdW  = 80; 
int  dvdH  = 30; 
unsigned long lastDvdMs = 0;
unsigned long dvdInterval = 35;

uint16_t dvdColors[] = {
  ST77XX_WHITE,
  CYBER_ACCENT,
  CYBER_LIGHT,
  CYBER_GREEN,
  CYBER_PINK,
  ST77XX_YELLOW
};
int dvdColorIndex = 0;

void drawDvdLogo(int x, int y, uint16_t c) {
  tft.fillRoundRect(x, y, dvdW, dvdH, 8, c);
  tft.drawRoundRect(x, y, dvdW, dvdH, 8, CYBER_BG);
  tft.setTextSize(2);
  tft.setTextColor(CYBER_BG, c);
  int16_t bx, by;
  uint16_t w, h;
  tft.getTextBounds("DVD", 0, 0, &bx, &by, &w, &h);
  int tx = x + (dvdW - (int)w) / 2;
  int ty = y + (dvdH - (int)h) / 2 + 1;
  tft.setCursor(tx, ty);
  tft.print("DVD");
}

void initDvd() {
  dvdInited = true;
  tft.fillScreen(CYBER_BG);
  drawAlarmIcon();
  dvdX = 80;
  dvdY = 80;
  dvdVX = 3;
  dvdVY = 2;
  lastDvdMs = millis();
  dvdColorIndex = 0;
  drawDvdLogo(dvdX, dvdY, dvdColors[dvdColorIndex]);
}

void updateDvd(int encStep, bool encPressed, bool backPressed) {
  (void)encPressed;

  if (backPressed) {
    dvdInited = false;
    currentMode = MODE_MENU;
    drawMenu();
    return;
  }

  if (encStep != 0) {
    int speed = abs(dvdVX) + encStep;
    if (speed < 1) speed = 1;
    if (speed > 8) speed = 8;
    dvdVX = (dvdVX >= 0 ? 1 : -1) * speed;
  }

  unsigned long now = millis();
  if (now - lastDvdMs < dvdInterval) return;
  lastDvdMs = now;

  tft.fillRoundRect(dvdX, dvdY, dvdW, dvdH, 8, CYBER_BG);

  dvdX += dvdVX;
  dvdY += dvdVY;

  int left   = 0;
  int right  = SCREEN_WIDTH - dvdW;
  int top    = 0; 
  int bottom = SCREEN_HEIGHT - dvdH;

  bool hitX = false;
  bool hitY = false;

  if (dvdX <= left) {
    dvdX = left;
    dvdVX = -dvdVX;
    hitX = true;
  } else if (dvdX >= right) {
    dvdX = right;
    dvdVX = -dvdVX;
    hitX = true;
  }

  if (dvdY <= top) {
    dvdY = top;
    dvdVY = -dvdVY;
    hitY = true;
  } else if (dvdY >= bottom) {
    dvdY = bottom;
    dvdVY = -dvdVY;
    hitY = true;
  }

  if (hitX && hitY) {
    dvdColorIndex++;
    if (dvdColorIndex >= (int)(sizeof(dvdColors) / sizeof(dvdColors[0]))) {
      dvdColorIndex = 0;
    }
    tone(BUZZ_PIN, 1500, 80);
  }

  drawDvdLogo(dvdX, dvdY, dvdColors[dvdColorIndex]);
}

// ========= SETUP =========
void setup() {
  Serial.begin(115200);
  delay(1500);

  // Initialize History with Sample Wave Data for Testing (4 Lines)
  for(int i=0; i<HISTORY_LEN; i++) {
    float a1 = (float)i / HISTORY_LEN * 3.14159 * 2;
    float a2 = (float)i / HISTORY_LEN * 3.14159 * 4;
    
    // Temp (0-50): sine from 20 to 30
    histTemp[i] = (uint16_t)((25 + 5*sin(a1)) * 10);
    
    // Hum (0-100): cosine from 50 to 80
    histHum[i]  = (uint16_t)(65 + 15*cos(a1 + 1));
    
    // TVOC (0-1000): fast wave 100-300
    histTVOC[i] = (uint16_t)(200 + 100*sin(a2));
    
    // CO2 (400-3000): slow wave 600-1200
    histCO2[i]  = (uint16_t)(900 + 300*cos(a1));
  }
  historyHead = 0; 

  pinMode(ENC_A_PIN,   INPUT_PULLUP);
  pinMode(ENC_B_PIN,   INPUT_PULLUP);
  pinMode(ENC_BTN_PIN, INPUT_PULLUP);
  pinMode(KEY0_PIN,    INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); 

  tft.init(240, 320); 
  tft.setRotation(1); 
  tft.invertDisplay(false);
  tft.fillScreen(CYBER_BG);

  connectWiFiAndSyncTime();

  if (!aht.begin()) Serial.println("AHT21 not found");
  if (!ens160.begin()) Serial.println("ENS160 begin FAIL");
  else ens160.setMode(ENS160_OPMODE_STD);

  updateEnvSensors(true);

  initClockStaticUI();
  prevTimeStr = "";
  drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
  drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
}

// ========= LOOP =========
void loop() {
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
    }
  }

  int encStep     = readEncoderStep();
  bool encPressed = checkButtonPressed(ENC_BTN_PIN, lastEncBtn);
  bool k0Pressed  = checkButtonPressed(KEY0_PIN,    lastKey0);

  checkAlarmTrigger();
  updateAlertStateAndLED();

  switch (currentMode) {
    case MODE_MENU: {
      if (encStep != 0) {
        menuIndex += encStep;
        if (menuIndex < 0) menuIndex = MENU_ITEMS - 1;
        if (menuIndex >= MENU_ITEMS) menuIndex = 0;
        drawMenu();
      }
      if (encPressed) {
        if (menuIndex == 0) {
          currentMode = MODE_CLOCK;
          initClockStaticUI();
          prevTimeStr = "";
          updateEnvSensors(true);
          drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
          drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
        } else if (menuIndex == 1) {
          currentMode = MODE_POMODORO;
          pomoState = POMO_SELECT;
          prevPomoRemainSec = -1;
          prevPomoPreset = -1;
          prevPomoStateInt = -1;
          drawPomodoroScreen(true);
        } else if (menuIndex == 2) {
          currentMode = MODE_ALARM;
          alarmSelectedField = 0;
          drawAlarmScreen(true);
        } else if (menuIndex == 3) {
          currentMode = MODE_DVD;
          dvdInited = false;
        } 
      }
      
      // Cancel menu with K0
      if (k0Pressed) {
        currentMode = MODE_CLOCK;
        initClockStaticUI();
        prevTimeStr = "";
        drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
        drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
      }
      break;
    }

    case MODE_CLOCK: {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        int sec = timeinfo.tm_sec;
        if (sec != prevSecond) {
          prevSecond = sec;
          drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
          
          if (sec % 5 == 0) {
            updateEnvSensors(true);
            drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
          } else {
             updateEnvSensors(false);
          }
        }
      }
      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }

    case MODE_POMODORO: {
      if (pomoState == POMO_SELECT && encStep != 0) {
        pomoPresetIndex += encStep;
        if (pomoPresetIndex < 0) pomoPresetIndex = 2;
        if (pomoPresetIndex > 2) pomoPresetIndex = 0;
        prevPomoRemainSec = -1;
        drawPomodoroScreen(true);
      }

      if (encPressed) {
        if (pomoState == POMO_SELECT || pomoState == POMO_DONE) {
          pomoState = POMO_RUNNING;
          pomoStartMillis = millis();
          prevPomoRemainSec = -1;
          drawPomodoroScreen(true);
        } else if (pomoState == POMO_RUNNING) {
          pomoState = POMO_PAUSED;
          pomoPausedMillis = millis();
          drawPomodoroScreen(true);
        } else if (pomoState == POMO_PAUSED) {
          unsigned long pauseDur = millis() - pomoPausedMillis;
          pomoStartMillis += pauseDur;
          pomoState = POMO_RUNNING;
          prevPomoRemainSec = -1;
          drawPomodoroScreen(true);
        }
      }

      if (pomoState == POMO_RUNNING || pomoState == POMO_PAUSED || pomoState == POMO_DONE) {
        unsigned long durationMs = pomoDurationsMin[pomoPresetIndex] * 60UL * 1000UL;
        unsigned long elapsed = 0;
        if (pomoState == POMO_RUNNING) elapsed = millis() - pomoStartMillis;
        else if (pomoState == POMO_PAUSED) elapsed = pomoPausedMillis - pomoStartMillis;
        else if (pomoState == POMO_DONE) elapsed = durationMs;

        if (elapsed > durationMs) {
          elapsed = durationMs;
          if (pomoState != POMO_DONE) {
            pomoState = POMO_DONE;
            tone(BUZZ_PIN, 2000, 500);
            drawPomodoroScreen(true);
          }
        }

        int remainSec = (durationMs - elapsed) / 1000UL;
        if (remainSec != prevPomoRemainSec) {
          prevPomoRemainSec = remainSec;
          drawPomodoroScreen(false);
        }
      }

      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }

    case MODE_ALARM: {
      if (alarmRinging) {
        static unsigned long lastBeep = 0;
        if (millis() - lastBeep > 1000) {
          lastBeep = millis();
          tone(BUZZ_PIN, 2000, 400);
        }
        if (encPressed || k0Pressed) {
          alarmRinging = false;
          lastAlarmDayTriggered = -1;
          noTone(BUZZ_PIN);
          drawAlarmScreen(true);
        }
        break;
      }

      bool changed = false;
      if (encStep != 0) {
        if (alarmSelectedField == 0) {
          if (encStep > 0) alarmHour = (alarmHour + 1) % 24;
          else             alarmHour = (alarmHour + 23) % 24;
          changed = true;
        } else if (alarmSelectedField == 1) {
          if (encStep > 0) alarmMinute = (alarmMinute + 1) % 60;
          else             alarmMinute = (alarmMinute + 59) % 60;
          changed = true;
        } else if (alarmSelectedField == 2) {
          alarmEnabled = !alarmEnabled;
          changed = true;
        }

        if (changed) {
          lastAlarmDayTriggered = -1;
        }
      }

      if (encPressed) {
        alarmSelectedField = (alarmSelectedField + 1) % 3;
        changed = true;
      }

      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
        break;
      }

      if (changed) {
        drawAlarmScreen(false);
        drawAlarmIcon();
      }

      break;
    }

    case MODE_DVD: {
      if (!dvdInited) {
        initDvd();
      }
      updateDvd(encStep, encPressed, k0Pressed);
      break;
    }
  }
}