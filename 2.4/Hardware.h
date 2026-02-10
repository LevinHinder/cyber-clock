#ifndef HARDWARE_H
#define HARDWARE_H

#include "Config.h"
#include "Globals.h"

void initHardware();
void setLedState(bool on);
void playSystemTone(unsigned int frequency, unsigned long durationMs = 0);
void stopSystemTone();
void updateEnvSensors(bool force = false);
void loadSettings();
void saveSettings();
void syncTime();
String getTimeStr(char type);
void updateAlertStateAndLED();

#endif