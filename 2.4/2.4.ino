#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>

#include "Config.h"
#include "Types.h"
#include "Globals.h"
#include "Hardware.h"
#include "Graphics.h"
#include "Modes.h"
#include "InputManager.h"

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    initHardware();
    loadSettings();

    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
    SPI.begin(Pins::TFT_SCLK, -1, Pins::TFT_MOSI, Pins::TFT_CS); 
    tft.init(Screen::HEIGHT, Screen::WIDTH); 
    tft.setRotation(1); 
    tft.invertDisplay(false);
    UI::clear();

    checkStartupWiFi();

    if (!aht.begin()) Serial.println("AHT21 not found");
    if (!ens160.begin()) Serial.println("ENS160 begin FAIL");
    else ens160.setMode(ENS160_OPMODE_STD);
    
    updateEnvSensors(true);
    initClockStaticUI();
    drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
    drawEnvDynamic();
}

void loop() {
    Input.update();
    checkAlarmTrigger();
    updateAlertStateAndLED();

    switch (ui.currentMode) {
        case MODE_MENU:                 runMenu(); break;
        case MODE_CLOCK:                runClock(); break;
        case MODE_POMODORO:             runPomodoro(); break;
        case MODE_ALARM:                runAlarm(); break;
        case MODE_DVD:                  runDvd(); break;
        case MODE_SETTINGS:             runSettingsMenu(); break;
        case MODE_SETTINGS_EDIT:        runSettingsEdit(); break;
        case MODE_WIFI_MENU:            runWiFiMenu(); break;
        case MODE_WIFI_SETUP:           runWiFiSetup(); break;
        case MODE_WIFI_RESET_CONFIRM:   runWiFiResetConfirm(); break;
    }
}