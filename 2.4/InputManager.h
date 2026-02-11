#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "Config.h"
#include <Arduino.h>

class InputManager {
  private:
    int lastEncA = HIGH;
    int lastEncB = HIGH;
    bool lastEncBtnState = HIGH;
    unsigned long lastEncBtnMs = 0;

  public:
    int encStep = 0;
    bool encPressed = false;
    
    // This flag is set by the Interrupt Service Routine (ISR)
    volatile bool backPressed = false;

    void begin();
    void update();
};

#endif