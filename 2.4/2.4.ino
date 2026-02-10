#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>

#include "Config.h"
#include "Types.h"
#include "Globals.h"
#include "Hardware.h"
#include "Graphics.h"
#include "StateManager.h"
#include "AppModes.h"
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
    
    State.switchMode(new ClockMode());
}

void loop() {
    Input.update();
    checkAlarmTrigger();
    updateAlertStateAndLED();

    if (ui.alarmRinging) {
        static bool wasRinging = false;
        if (!wasRinging) {
             State.switchMode(new AlarmMode(true));
             wasRinging = true;
        }
        if (!ui.alarmRinging) wasRinging = false; 
    }

    State.update();
}