#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <Arduino.h>
#include "Config.h"

class InputManager {
private:
    int lastEncA = HIGH;
    int lastEncB = HIGH;
    bool lastEncBtnState = HIGH;
    bool lastKey0State = HIGH;
    unsigned long lastEncBtnMs = 0;
    unsigned long lastKey0Ms = 0;
    bool key0Consumed = false;

public:
    // Public state to be read by Modes
    int encStep = 0;
    bool encPressed = false;
    bool backPressed = false;

    void begin();
    void update();
};

#endif