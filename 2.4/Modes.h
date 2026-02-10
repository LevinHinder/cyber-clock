#ifndef MODES_H
#define MODES_H

#include "Globals.h"
#include "Graphics.h"
#include "Hardware.h"

void runClock();
void runMenu();
void runPomodoro();
void runAlarm();
void runDvd();
void runSettingsMenu();
void runSettingsEdit();
void runWiFiMenu();
void runWiFiResetConfirm();
void runWiFiSetup();
void checkAlarmTrigger();
void checkStartupWiFi();

#endif