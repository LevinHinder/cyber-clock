#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> 
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include <Preferences.h> 
#include "time.h"

const char* ssid = "Hinder WLAN";
const char* password = "t&5yblzK6*e#";
const char* ntpServer = "pool.ntp.org";
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3"; 

#define TFT_CS    9
#define TFT_DC    8
#define TFT_RST   7
#define TFT_MOSI  6
#define TFT_SCLK  5

#define ENC_A_PIN    10
#define ENC_B_PIN    20
#define ENC_BTN_PIN  21
#define KEY0_PIN     0

#define SDA_PIN 1
#define SCL_PIN 2

#define LED_PIN 4
#define BUZZ_PIN 3

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_CX     (SCREEN_WIDTH / 2)
#define SCREEN_CY     (SCREEN_HEIGHT / 2)

#define CYBER_BG      ST77XX_BLACK
#define CYBER_GREEN   0x07E0  
#define CYBER_ACCENT  0x07FF  
#define CYBER_LIGHT   0xFD20  
#define CYBER_BLUE    0x07FF  
#define CYBER_PINK    0xF81F
#define CYBER_DARK    0x4208
#define GRID_COLOR    0x2104 

#define AQ_BAR_GREEN  0x07E0
#define AQ_BAR_YELLOW 0xFFE0
#define AQ_BAR_ORANGE 0xFD20
#define AQ_BAR_RED    0xF800

#define COL_TEMP      ST77XX_RED
#define COL_HUM       CYBER_BLUE
#define COL_TVOC      CYBER_GREEN
#define COL_CO2       ST77XX_YELLOW 

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 graphCanvas(320, 90);
Adafruit_AHTX0 aht;
ScioSense_ENS160 ens160(0x53);
Preferences prefs; 

// --- Settings Globals ---
int settingLedBrightness = 100; 
int settingSpeakerVol = 100;    

int prevLedBarW = -1;
int prevSpkBarW = -1;

enum UIMode { MODE_MENU = 0, MODE_CLOCK, MODE_POMODORO, MODE_ALARM, MODE_DVD, MODE_SETTINGS };
UIMode currentMode = MODE_CLOCK;      

int menuIndex = 0;
const int MENU_ITEMS = 5; 
const char* menuLabels[MENU_ITEMS] = { "Monitor", "Pomodoro", "Alarm", "DVD", "Settings" };

enum PomodoroState { POMO_SET_WORK = 0, POMO_SET_SHORT, POMO_SET_LONG, POMO_SET_CYCLES, POMO_READY, POMO_RUNNING, POMO_PAUSED, POMO_DONE };
enum PomoPhase { PHASE_WORK, PHASE_SHORT, PHASE_LONG };

PomodoroState pomoState = POMO_SET_WORK;
PomoPhase pomoPhase = PHASE_WORK;

int cfgWorkMin = 25;
int cfgShortMin = 5;
int cfgLongMin = 15;
int cfgCycles = 4;

int currentCycle = 1;
unsigned long pomoStartMillis = 0;
unsigned long pomoPausedMillis = 0;
unsigned long currentDurationMs = 0;
int prevPomoVal = -1; 
int prevBarWidth = -1; 
String prevPomoLabel = ""; // To track label changes

float curTemp = 0;
float curHum = 0;
uint16_t curTVOC = 0;
uint16_t curECO2 = 400;
unsigned long lastEnvRead = 0;

#define HISTORY_LEN 320 
uint16_t histTemp[HISTORY_LEN];
uint16_t histHum[HISTORY_LEN];
uint16_t histTVOC[HISTORY_LEN];
uint16_t histCO2[HISTORY_LEN];
int historyHead = 0;
unsigned long lastHistoryAdd = 0;
const unsigned long HISTORY_INTERVAL = 1000; 

int prevSecond = -1;
String prevTimeStr = "";

int lastEncA = HIGH;
int lastEncB = HIGH;
bool lastEncBtn = HIGH;
bool lastKey0 = HIGH;
unsigned long lastBtnMs = 0;

bool alarmEnabled = false;
uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;
bool alarmRinging = false;
int alarmSelectedField = 0; 
int lastAlarmDayTriggered = -1;

// Settings Logic
int settingsSelectedRow = 0; // 0 = LED, 1 = Speaker

enum AlertLevel { ALERT_NONE = 0, ALERT_CO2, ALERT_ALARM };
AlertLevel currentAlertLevel = ALERT_NONE;
unsigned long lastLedToggleMs = 0;
bool ledState = false;
unsigned long lastCo2BlinkMs = 0;
bool co2BlinkOn = false;

const int GRID_L = 10;
const int GRID_R = 310;
const int GRID_MID_X = (GRID_L + GRID_R) / 2;
const int GRID_TOP = 75; 
const int GRID_MID = 107;
const int GRID_BOT = 139;
const int TOP_LABEL_Y = GRID_TOP + 4;  
const int TOP_VALUE_Y = GRID_TOP + 14; 
const int BOTTOM_LABEL_Y = GRID_MID + 4;  
const int BOTTOM_VALUE_Y = GRID_MID + 14; 

bool dvdInited = false;
int dvdX, dvdY, dvdVX = 2, dvdVY = 1;
int dvdW = 80, dvdH = 30; 
unsigned long lastDvdMs = 0;
unsigned long dvdInterval = 35;
uint16_t dvdColors[] = { ST77XX_WHITE, CYBER_ACCENT, CYBER_LIGHT, CYBER_GREEN, CYBER_PINK, ST77XX_YELLOW };
int dvdColorIndex = 0;

// Forward Declarations
void drawHistoryGraph(); 
void drawMenu(bool fullRedraw);
void initPomodoroWizard(const char* title);
void updatePomodoroValue(int value);
void drawPomodoroScreen(bool forceStatic);
void drawAlarmScreen(bool full);
void drawAlarmRingingScreen();
void drawSettingsScreen(bool full);

// --- PERSISTENCE HELPERS ---
void loadSettings() {
  prefs.begin("cyber", true); 
  settingLedBrightness = prefs.getInt("led_b", 100);
  settingSpeakerVol = prefs.getInt("spk_v", 100);
  
  alarmHour = prefs.getInt("alm_h", 7);
  alarmMinute = prefs.getInt("alm_m", 0);
  alarmEnabled = prefs.getBool("alm_e", false);
  
  cfgWorkMin = prefs.getInt("p_work", 25);
  cfgShortMin = prefs.getInt("p_short", 5);
  cfgLongMin = prefs.getInt("p_long", 15);
  cfgCycles = prefs.getInt("p_cycl", 4);
  prefs.end();
}

void saveSettings() {
  prefs.begin("cyber", false); 
  prefs.putInt("led_b", settingLedBrightness);
  prefs.putInt("spk_v", settingSpeakerVol);
  
  prefs.putInt("alm_h", alarmHour);
  prefs.putInt("alm_m", alarmMinute);
  prefs.putBool("alm_e", alarmEnabled);
  
  prefs.putInt("p_work", cfgWorkMin);
  prefs.putInt("p_short", cfgShortMin);
  prefs.putInt("p_long", cfgLongMin);
  prefs.putInt("p_cycl", cfgCycles);
  prefs.end();
}

// --- HELPER: SYSTEM TONE ---
void playSystemTone(unsigned int frequency, unsigned long durationMs = 0) {
  if (settingSpeakerVol == 0) {
     ledcWrite(BUZZ_PIN, 0);
     return;
  }
  ledcAttach(BUZZ_PIN, frequency, 8);
  int duty = map(settingSpeakerVol, 0, 100, 0, 128);
  ledcWrite(BUZZ_PIN, duty);
  if (durationMs > 0) {
    delay(durationMs);
    ledcWrite(BUZZ_PIN, 0);
  }
}

void stopSystemTone() {
  ledcWrite(BUZZ_PIN, 0);
}

// --- HELPER: LED CONTROL ---
void setLedState(bool on) {
  if (on) {
    int duty = map(settingLedBrightness, 0, 100, 0, 255);
    ledcWrite(LED_PIN, duty);
  } else {
    ledcWrite(LED_PIN, 0);
  }
}

int readEncoderStep() {
  int encA = digitalRead(ENC_A_PIN);
  int encB = digitalRead(ENC_B_PIN);
  int step = 0;
  if (encA != lastEncA) {
    if (encA == LOW) {
      if (encB == HIGH) step = +1;
      else step = -1;
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
    tft.fillScreen(CYBER_BG);
    tft.setCursor(20, 100);
    tft.print("Syncing time...");
    configTime(0, 0, ntpServer);
    setenv("TZ", time_zone, 1);
    tzset();

    struct tm timeinfo;
    int retrySync = 0;
    while (!getLocalTime(&timeinfo) && retrySync < 10) {
      tft.print(".");
      delay(500);
      retrySync++;
    }
    delay(500);
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

void recordHistory(float t, float h, uint16_t tvoc, uint16_t eco2) {
  histTemp[historyHead] = (uint16_t)(t * 10); 
  histHum[historyHead]  = (uint16_t)h;
  histTVOC[historyHead] = tvoc;
  histCO2[historyHead]  = eco2;
  historyHead = (historyHead + 1) % HISTORY_LEN;
  if (currentMode == MODE_CLOCK) drawHistoryGraph();
}

void updateEnvSensors(bool force = false) {
  unsigned long now = millis();
  if (force || (now - lastEnvRead) >= 1000) { 
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
  if (now - lastHistoryAdd >= HISTORY_INTERVAL) {
    lastHistoryAdd = now;
    recordHistory(curTemp, curHum, curTVOC, curECO2);
  }
}

void drawHistoryGraph() {
  graphCanvas.fillScreen(CYBER_BG);
  int h = 90;
  graphCanvas.drawFastHLine(0, h/4,   320, GRID_COLOR);
  graphCanvas.drawFastHLine(0, h/2,   320, GRID_COLOR);
  graphCanvas.drawFastHLine(0, 3*h/4, 320, GRID_COLOR);

  int pX = -1;
  int pY_T = -1, pY_H = -1, pY_V = -1, pY_C = -1;

  for (int i = 0; i < HISTORY_LEN; i++) {
    int idx = (historyHead + i) % HISTORY_LEN;
    
    int valT = histTemp[idx] / 10; 
    int yT = map(constrain(valT, 0, 50), 0, 50, 88, 2);
    int valH = histHum[idx];
    int yH = map(constrain(valH, 0, 100), 0, 100, 88, 2);
    int valV = histTVOC[idx];
    int yV = map(constrain(valV, 0, 750), 0, 600, 88, 2);
    int valC = histCO2[idx];
    int yC = map(constrain(valC, 400, 1200), 400, 1200, 88, 2);
    int x = i;

    if (i > 0) {
        graphCanvas.drawLine(pX, pY_T, x, yT, COL_TEMP); 
        graphCanvas.drawLine(pX, pY_H, x, yH, COL_HUM);  
        graphCanvas.drawLine(pX, pY_V, x, yV, COL_TVOC); 
        graphCanvas.drawLine(pX, pY_C, x, yC, COL_CO2); 
    }
    pX = x; pY_T = yT; pY_H = yH; pY_V = yV; pY_C = yC;
  }
  graphCanvas.drawRect(0, 0, 320, 90, GRID_COLOR);
  tft.drawRGBBitmap(0, 145, graphCanvas.getBuffer(), 320, 90);
}

void initClockStaticUI() {
  tft.fillScreen(CYBER_BG);
  drawAlarmIcon();
  tft.drawFastHLine(GRID_L, GRID_TOP, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastHLine(GRID_L, GRID_MID, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastHLine(GRID_L, GRID_BOT, GRID_R - GRID_L, ST77XX_WHITE);
  tft.drawFastVLine(GRID_MID_X, GRID_TOP, GRID_BOT - GRID_TOP, ST77XX_WHITE);

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

  int16_t x1, y1; uint16_t w, h;
  tft.setTextSize(6); 
  tft.getTextBounds("88:88:88", 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = 10; 
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.setCursor(x, y);
  tft.print(cur);
}

void drawEnvDynamic(float temp, float hum, uint16_t tvoc, uint16_t eco2) {
  int clearH = 18; 
  int w_left  = (GRID_MID_X - GRID_L) - 2;
  int w_right = (GRID_R - GRID_MID_X) - 2;

  tft.fillRect(GRID_L + 1, TOP_VALUE_Y, w_left, clearH, CYBER_BG);
  char humBuf[8]; sprintf(humBuf, "%2.0f%%", hum);
  printCenteredText(String(humBuf), GRID_L, GRID_MID_X, TOP_VALUE_Y, COL_HUM, CYBER_BG, 2);

  tft.fillRect(GRID_MID_X + 1, TOP_VALUE_Y, w_right, clearH, CYBER_BG);
  char tempBuf[10]; sprintf(tempBuf, "%2.1fC", temp);
  printCenteredText(String(tempBuf), GRID_MID_X, GRID_R, TOP_VALUE_Y, COL_TEMP, CYBER_BG, 2);

  tft.fillRect(GRID_L + 1, BOTTOM_VALUE_Y, w_left, clearH, CYBER_BG);
  char tvocBuf[16]; sprintf(tvocBuf, "%d", tvoc);
  printCenteredText(String(tvocBuf), GRID_L, GRID_MID_X, BOTTOM_VALUE_Y, COL_TVOC, CYBER_BG, 2);

  tft.fillRect(GRID_MID_X + 1, BOTTOM_VALUE_Y, w_right, clearH, CYBER_BG);
  char co2Buf[12]; sprintf(co2Buf, "%d", eco2);
  printCenteredText(String(co2Buf), GRID_MID_X, GRID_R, BOTTOM_VALUE_Y, COL_CO2, CYBER_BG, 2);
}

void drawMenuItem(int index, bool selected) {
  if (index < 0 || index >= MENU_ITEMS) return;
  int rowCenterY = 60 + index * 35; 
  int boxY = rowCenterY - 14;
  int boxH = 28;
  int boxW = 300;
  int boxX = 10;
  int textY = rowCenterY - 7;
  int textX = 24;

  if (selected) {
    tft.fillRect(boxX, boxY, boxW, boxH, CYBER_ACCENT);
    tft.setTextColor(CYBER_BG);
    tft.setTextSize(2);
    tft.setCursor(textX, textY);
    tft.print(menuLabels[index]);
  } else {
    tft.fillRect(boxX, boxY, boxW, boxH, CYBER_BG);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(textX, textY);
    tft.print(menuLabels[index]);
  }
}

void drawMenu(bool fullRedraw) {
  if (fullRedraw) {
    tft.fillScreen(CYBER_BG);
    drawAlarmIcon();
    for (int i = 0; i < MENU_ITEMS; i++) {
      drawMenuItem(i, (i == menuIndex));
    }
  }
}

void initPomodoroWizard(const char* title) {
  tft.fillScreen(CYBER_BG);
  tft.setTextSize(2);
  tft.setTextColor(CYBER_LIGHT, CYBER_BG);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_WIDTH - w)/2, 40);
  tft.print(title);
}

void updatePomodoroValue(int value) {
  if (value == prevPomoVal) return;
  prevPomoVal = value;
  int numY = 100;
  int numH = 50; 
  int numW = 100;
  int startX = (SCREEN_WIDTH - numW) / 2;
  
  tft.fillRect(startX, numY, numW, numH, CYBER_BG);
  tft.setTextSize(6);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  String valStr = String(value);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(valStr, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_WIDTH - w)/2, numY);
  tft.print(valStr);
}

void drawPomodoroBar(float progress) {
  int barX = 20;
  int barY = 225; 
  int barW = 280;
  int barH = 10;

  if (prevBarWidth == -1) {
    tft.drawRect(barX - 1, barY - 1, barW + 2, barH + 2, ST77XX_WHITE);
    tft.fillRect(barX, barY, barW, barH, CYBER_DARK);
    prevBarWidth = 0;
  }

  int targetW = (int)(barW * progress);
  if (targetW != prevBarWidth) {
    if (targetW > prevBarWidth) {
      tft.fillRect(barX + prevBarWidth, barY, targetW - prevBarWidth, barH, CYBER_GREEN);
    } else {
      tft.fillRect(barX + targetW, barY, prevBarWidth - targetW, barH, CYBER_DARK);
    }
    prevBarWidth = targetW;
  }
}

void drawPomodoroScreen(bool forceStatic) {
  // If forcing a static redraw, clear screen and reset trackers
  if (forceStatic) {
    tft.fillScreen(CYBER_BG);
    drawAlarmIcon();
    prevBarWidth = -1; 
    prevPomoLabel = "";
    prevTimeStr = ""; // Force time redraw
    
    char cycleBuf[20];
    sprintf(cycleBuf, "Cycle: %d/%d", currentCycle, cfgCycles);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE, CYBER_BG);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(cycleBuf, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SCREEN_WIDTH - w)/2, 190);
    tft.print(cycleBuf);
  }

  // --- 1. HANDLE STATUS LABEL (NO FLICKER) ---
  String labelStr = "";
  uint16_t labelColor = CYBER_LIGHT;

  if (pomoState == POMO_PAUSED) {
    labelStr = "[ PAUSED ]";
    labelColor = ST77XX_YELLOW;
  } else {
    if (pomoPhase == PHASE_WORK) {
      labelStr = "GET TO WORK!";
      labelColor = CYBER_ACCENT;
    } else if (pomoPhase == PHASE_SHORT) {
      labelStr = "SHORT BREAK";
      labelColor = CYBER_GREEN;
    } else {
      labelStr = "LONG BREAK";
      labelColor = CYBER_BLUE;
    }
  }

  if (labelStr != prevPomoLabel) {
    // Clear the specific area where the text goes
    // Assuming max width ~200px, height ~20px centered at Y=30
    tft.fillRect(0, 30, SCREEN_WIDTH, 20, CYBER_BG); 
    
    tft.setTextSize(2);
    tft.setTextColor(labelColor, CYBER_BG);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(labelStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((SCREEN_WIDTH - w)/2, 30);
    tft.print(labelStr);
    
    prevPomoLabel = labelStr;
  }

  // --- 2. CALCULATE TIME & PROGRESS ---
  unsigned long elapsed = 0;
  if (pomoState == POMO_RUNNING) elapsed = millis() - pomoStartMillis;
  else if (pomoState == POMO_PAUSED) elapsed = pomoPausedMillis - pomoStartMillis;
  if (elapsed > currentDurationMs) elapsed = currentDurationMs;

  float progress = (currentDurationMs > 0) ? (float)elapsed / currentDurationMs : 0.0f;
  drawPomodoroBar(progress);

  unsigned long remain = currentDurationMs - elapsed;
  uint16_t rmMin = remain / 60000UL;
  uint8_t  rmSec = (remain / 1000UL) % 60;
  char buf[8];
  sprintf(buf, "%02d:%02d", rmMin, rmSec);

  // --- 3. DRAW TIMER (Only if changed) ---
  String timeStr = String(buf);
  // Also force redraw if state changed (to change color)
  bool stateChanged = (pomoState == POMO_PAUSED && prevTimeStr.indexOf(":") != -1) || 
                      (pomoState == POMO_RUNNING && prevTimeStr.indexOf(":") != -1);

  // Determine Timer Color
  uint16_t timeColor = (pomoState == POMO_PAUSED) ? CYBER_LIGHT : ST77XX_WHITE;

  // We check prevTimeStr to avoid redrawing every loop, but we also check
  // if we need to repaint because the color changed (pause <-> run).
  // Note: prevTimeStr tracks the *text*, not color. So we might need to force it.
  // Ideally, use a separate 'prevTimeColor'. For now, we rely on the loop.
  
  // Actually, standard GFX setTextColor(FG, BG) handles overwrite if BG is set.
  // We just need to ensure we print if text OR color changes.
  
  static uint16_t prevTimeColor = 0;
  
  if (timeStr != prevTimeStr || timeColor != prevTimeColor) {
     tft.setTextSize(6); 
     tft.setTextColor(timeColor, CYBER_BG); 
     int16_t x1, y1; uint16_t w, h;
     tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
     // We fillRect the timer area to be safe against different character widths (e.g. 1 vs 7)
     // 60x50 area centered at 90
     // tft.fillRect(40, 90, 240, 50, CYBER_BG); // Optional, helps if font is proportional
     tft.setCursor((SCREEN_WIDTH - w)/2, 90);
     tft.print(timeStr);
     
     prevTimeStr = timeStr;
     prevTimeColor = timeColor;
  }
}

void drawAlarmScreen(bool full) {
  if (full) {
    tft.fillScreen(CYBER_BG);
    drawAlarmIcon();
  }
  char hBuf[3], mBuf[3];
  sprintf(hBuf, "%02d", alarmHour);
  sprintf(mBuf, "%02d", alarmMinute);

  tft.setTextSize(6);
  String disp = String(hBuf) + ":" + String(mBuf);
  int16_t x1, y1; uint16_t w, h;
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
  uint16_t enColor = alarmSelectedField == 2 ? CYBER_LIGHT : (alarmEnabled ? CYBER_GREEN : ST77XX_RED);
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

// --- UPDATED SETTINGS DRAWING ---
void drawBarItem(int x, int y, int w, int h, int value, int &prevW) {
    tft.drawRect(x - 1, y - 1, w + 2, h + 2, ST77XX_WHITE);
    
    if (prevW == -1) {
        tft.fillRect(x, y, w, h, CYBER_DARK);
        prevW = 0;
    }

    int targetW = (int)((float)w * (value / 100.0f));
    
    if (targetW != prevW) {
        if (targetW > prevW) {
             tft.fillRect(x + prevW, y, targetW - prevW, h, CYBER_GREEN);
        } else {
             tft.fillRect(x + targetW, y, prevW - targetW, h, CYBER_DARK);
        }
        prevW = targetW;
    }
}

void drawSettingsScreen(bool full) {
  if (full) {
    tft.fillScreen(CYBER_BG);
    drawAlarmIcon();
    prevLedBarW = -1;
    prevSpkBarW = -1;
  }

  int barX = 40;
  int barW = 240;
  int barH = 15;
  
  // LED Section
  int yLed = 80;
  tft.setTextSize(2);
  tft.setTextColor(settingsSelectedRow == 0 ? CYBER_ACCENT : ST77XX_WHITE, CYBER_BG);
  tft.setCursor(barX, yLed - 20);
  tft.print("LED Brightness");
  
  drawBarItem(barX, yLed, barW, barH, settingLedBrightness, prevLedBarW);

  // Speaker Section
  int ySpk = 160;
  tft.setTextColor(settingsSelectedRow == 1 ? CYBER_ACCENT : ST77XX_WHITE, CYBER_BG);
  tft.setCursor(barX, ySpk - 20);
  tft.print("Speaker Volume");
  
  drawBarItem(barX, ySpk, barW, barH, settingSpeakerVol, prevSpkBarW);
}

void checkAlarmTrigger() {
  if (!alarmEnabled || alarmRinging) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && timeinfo.tm_sec == 0) {
    alarmRinging = true;
    lastAlarmDayTriggered = timeinfo.tm_mday;
    currentMode = MODE_ALARM;
    drawAlarmRingingScreen();
  }
}

void updateAlertStateAndLED() {
  if (alarmRinging) currentAlertLevel = ALERT_ALARM;
  else if (curECO2 > 1800) currentAlertLevel = ALERT_CO2;
  else currentAlertLevel = ALERT_NONE;

  unsigned long now = millis();
  if (currentAlertLevel == ALERT_NONE) {
    if (currentMode != MODE_SETTINGS) {
        setLedState(false);
    }
    ledState = false; 
  } else {
    unsigned long interval;
    if (currentAlertLevel == ALERT_ALARM) interval = 120; 
    else interval = 250; 
    if (now - lastLedToggleMs > interval) {
      lastLedToggleMs = now;
      ledState = !ledState;
      setLedState(ledState);
    }
  }

  if (currentAlertLevel == ALERT_CO2) {
    if (now - lastCo2BlinkMs > 350) {
      lastCo2BlinkMs = now;
      co2BlinkOn = !co2BlinkOn;
      playSystemTone(1800, 80); 
    }
  }
}

void drawDvdLogo(int x, int y, uint16_t c) {
  tft.fillRoundRect(x, y, dvdW, dvdH, 8, c);
  tft.drawRoundRect(x, y, dvdW, dvdH, 8, CYBER_BG);
  tft.setTextSize(2);
  tft.setTextColor(CYBER_BG, c);
  int16_t bx, by; uint16_t w, h;
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
  dvdX = 80; dvdY = 80; dvdVX = 3; dvdVY = 2;
  lastDvdMs = millis();
  dvdColorIndex = 0;
  drawDvdLogo(dvdX, dvdY, dvdColors[dvdColorIndex]);
}

void updateDvd(int encStep, bool encPressed, bool backPressed) {
  if (backPressed) {
    dvdInited = false;
    currentMode = MODE_MENU;
    drawMenu(true);
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

  int left = 0, right = SCREEN_WIDTH - dvdW;
  int top = 0, bottom = SCREEN_HEIGHT - dvdH;
  bool hitX = false, hitY = false;

  if (dvdX <= left) { dvdX = left; dvdVX = -dvdVX; hitX = true; } 
  else if (dvdX >= right) { dvdX = right; dvdVX = -dvdVX; hitX = true; }
  if (dvdY <= top) { dvdY = top; dvdVY = -dvdVY; hitY = true; } 
  else if (dvdY >= bottom) { dvdY = bottom; dvdVY = -dvdVY; hitY = true; }

  if (hitX && hitY) {
    dvdColorIndex = (dvdColorIndex + 1) % (sizeof(dvdColors) / sizeof(dvdColors[0]));
    playSystemTone(1500, 80); 
  }
  drawDvdLogo(dvdX, dvdY, dvdColors[dvdColorIndex]);
}

void runMenu(int encStep, bool encPressed, bool k0Pressed) {
    if (encStep != 0) {
        int prevMenuIndex = menuIndex;
        menuIndex += encStep;
        if (menuIndex < 0) menuIndex = MENU_ITEMS - 1;
        if (menuIndex >= MENU_ITEMS) menuIndex = 0;
        drawMenuItem(prevMenuIndex, false);
        drawMenuItem(menuIndex, true);
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
            pomoState = POMO_SET_WORK;
            prevPomoVal = -1;
            initPomodoroWizard("Set Work");
            updatePomodoroValue(cfgWorkMin);
        } else if (menuIndex == 2) {
            currentMode = MODE_ALARM;
            alarmSelectedField = 0;
            drawAlarmScreen(true);
        } else if (menuIndex == 3) {
            currentMode = MODE_DVD;
            dvdInited = false;
        } else if (menuIndex == 4) {
            currentMode = MODE_SETTINGS;
            settingsSelectedRow = 0; 
            setLedState(true);       
            drawSettingsScreen(true);
        }
    }
    if (k0Pressed) {
        currentMode = MODE_CLOCK;
        initClockStaticUI();
        prevTimeStr = "";
        drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
        drawEnvDynamic(curTemp, curHum, curTVOC, curECO2);
    }
}

void runClock(bool k0Pressed) {
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
        drawMenu(true);
    }
}

void runPomodoro(int encStep, bool encPressed, bool k0Pressed) {
    if (pomoState == POMO_SET_WORK) {
        if (encStep != 0) {
            cfgWorkMin = constrain(cfgWorkMin + encStep, 1, 90);
            updatePomodoroValue(cfgWorkMin);
        }
        if (encPressed) {
            pomoState = POMO_SET_SHORT;
            prevPomoVal = -1;
            initPomodoroWizard("Short Break");
            updatePomodoroValue(cfgShortMin);
        }
    } else if (pomoState == POMO_SET_SHORT) {
        if (encStep != 0) {
            cfgShortMin = constrain(cfgShortMin + encStep, 1, 30);
            updatePomodoroValue(cfgShortMin);
        }
        if (encPressed) {
            pomoState = POMO_SET_LONG;
            prevPomoVal = -1;
            initPomodoroWizard("Long Break");
            updatePomodoroValue(cfgLongMin);
        }
    } else if (pomoState == POMO_SET_LONG) {
        if (encStep != 0) {
            cfgLongMin = constrain(cfgLongMin + encStep, 1, 60);
            updatePomodoroValue(cfgLongMin);
        }
        if (encPressed) {
            pomoState = POMO_SET_CYCLES;
            prevPomoVal = -1;
            initPomodoroWizard("Set Cycles");
            updatePomodoroValue(cfgCycles);
        }
    } else if (pomoState == POMO_SET_CYCLES) {
        if (encStep != 0) {
            cfgCycles = constrain(cfgCycles + encStep, 1, 10);
            updatePomodoroValue(cfgCycles);
        }
        if (encPressed) {
            pomoState = POMO_RUNNING;
            pomoPhase = PHASE_WORK;
            currentCycle = 1;
            currentDurationMs = cfgWorkMin * 60UL * 1000UL;
            pomoStartMillis = millis();
            drawPomodoroScreen(true);
        }
    } else if (pomoState == POMO_RUNNING || pomoState == POMO_PAUSED) {
        if (encPressed) {
            if (pomoState == POMO_RUNNING) {
                pomoState = POMO_PAUSED;
                pomoPausedMillis = millis();
                // NO FULL REDRAW HERE - Flicker Fix
                // Just let the next loop handle the state change visuals
            } else {
                pomoStartMillis += (millis() - pomoPausedMillis);
                pomoState = POMO_RUNNING;
                // NO FULL REDRAW HERE either
            }
        }
        if (pomoState == POMO_RUNNING) {
            unsigned long elapsed = millis() - pomoStartMillis;
            if (elapsed >= currentDurationMs) {
                setLedState(true);
                playSystemTone(2000, 0); 
                delay(3000);
                stopSystemTone();
                setLedState(false);

                if (pomoPhase == PHASE_WORK) {
                    if (currentCycle < cfgCycles) {
                        pomoPhase = PHASE_SHORT;
                        currentDurationMs = cfgShortMin * 60UL * 1000UL;
                    } else {
                        pomoPhase = PHASE_LONG;
                        currentDurationMs = cfgLongMin * 60UL * 1000UL;
                    }
                } else if (pomoPhase == PHASE_SHORT) {
                    pomoPhase = PHASE_WORK;
                    currentCycle++;
                    currentDurationMs = cfgWorkMin * 60UL * 1000UL;
                } else if (pomoPhase == PHASE_LONG) {
                    pomoState = POMO_DONE;
                    drawPomodoroScreen(true);
                }
                pomoStartMillis = millis();
                if (pomoState != POMO_DONE) drawPomodoroScreen(true);
            } else {
                drawPomodoroScreen(false);
            }
        } else if (pomoState == POMO_PAUSED) {
             // Continue drawing screen (for label update) but time won't advance
             drawPomodoroScreen(false);
        }
    } else if (pomoState == POMO_DONE) {
        if (encPressed) {
            pomoState = POMO_SET_WORK;
            prevPomoVal = -1;
            initPomodoroWizard("Set Work");
            updatePomodoroValue(cfgWorkMin);
        }
    }

    if (k0Pressed) {
        saveSettings(); 
        currentMode = MODE_MENU;
        drawMenu(true);
    }
}

void runAlarm(int encStep, bool encPressed, bool k0Pressed) {
    if (alarmRinging) {
        static unsigned long lastBeep = 0;
        if (millis() - lastBeep > 1000) {
            lastBeep = millis();
            playSystemTone(2000, 400); 
        }
        if (encPressed || k0Pressed) {
            alarmRinging = false;
            lastAlarmDayTriggered = -1;
            stopSystemTone();
            drawAlarmScreen(true);
        }
        return;
    }

    bool changed = false;
    if (encStep != 0) {
        if (alarmSelectedField == 0) {
            alarmHour = (alarmHour + (encStep > 0 ? 1 : 23)) % 24;
            changed = true;
        } else if (alarmSelectedField == 1) {
            alarmMinute = (alarmMinute + (encStep > 0 ? 1 : 59)) % 60;
            changed = true;
        } else if (alarmSelectedField == 2) {
            alarmEnabled = !alarmEnabled;
            changed = true;
        }
        if (changed) lastAlarmDayTriggered = -1;
    }
    if (encPressed) {
        alarmSelectedField = (alarmSelectedField + 1) % 3;
        changed = true;
    }
    if (k0Pressed) {
        saveSettings(); 
        currentMode = MODE_MENU;
        drawMenu(true);
        return;
    }
    if (changed) {
        drawAlarmScreen(false);
        drawAlarmIcon();
    }
}

// --- UPDATED SETTINGS LOGIC ---
void runSettings(int encStep, bool encPressed, bool k0Pressed) {
    if (k0Pressed) {
        saveSettings(); 
        if (currentAlertLevel == ALERT_NONE) setLedState(false);
        currentMode = MODE_MENU;
        drawMenu(true);
        return;
    }

    if (encPressed) {
        settingsSelectedRow = !settingsSelectedRow; 
        if (settingsSelectedRow == 0) {
             setLedState(true);
        } else {
             setLedState(false);
        }
        drawSettingsScreen(false); 
    }

    if (encStep != 0) {
        if (settingsSelectedRow == 0) {
            settingLedBrightness = constrain(settingLedBrightness + (encStep * 5), 0, 100);
            setLedState(true); 
        } else {
            settingSpeakerVol = constrain(settingSpeakerVol + (encStep * 5), 0, 100);
            playSystemTone(2000, 50); 
        }
        drawSettingsScreen(false); 
    }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  for(int i=0; i<HISTORY_LEN; i++) {
    histTemp[i] = 0; histHum[i] = 0; histTVOC[i] = 0; histCO2[i] = 400; 
  }
  historyHead = 0; 

  pinMode(ENC_A_PIN,   INPUT_PULLUP);
  pinMode(ENC_B_PIN,   INPUT_PULLUP);
  pinMode(ENC_BTN_PIN, INPUT_PULLUP);
  pinMode(KEY0_PIN,    INPUT_PULLUP);
  
  ledcAttach(LED_PIN, 5000, 8);  
  ledcAttach(BUZZ_PIN, 2000, 8); 

  loadSettings();

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

void loop() {
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.begin(ssid, password);
  }

  int encStep = readEncoderStep();
  bool encPressed = checkButtonPressed(ENC_BTN_PIN, lastEncBtn);
  bool k0Pressed = checkButtonPressed(KEY0_PIN, lastKey0);

  checkAlarmTrigger();
  updateAlertStateAndLED();

  switch (currentMode) {
    case MODE_MENU:     runMenu(encStep, encPressed, k0Pressed); break;
    case MODE_CLOCK:    runClock(k0Pressed); break;
    case MODE_POMODORO: runPomodoro(encStep, encPressed, k0Pressed); break;
    case MODE_ALARM:    runAlarm(encStep, encPressed, k0Pressed); break;
    case MODE_DVD:      if (!dvdInited) initDvd(); updateDvd(encStep, encPressed, k0Pressed); break;
    case MODE_SETTINGS: runSettings(encStep, encPressed, k0Pressed); break;
  }
}