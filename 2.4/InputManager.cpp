#include "InputManager.h"

void InputManager::begin() {
    pinMode(Pins::ENC_A,   INPUT_PULLUP);
    pinMode(Pins::ENC_B,   INPUT_PULLUP);
    pinMode(Pins::ENC_BTN, INPUT_PULLUP);
    pinMode(Pins::KEY0,    INPUT_PULLUP);
}

void InputManager::update() {
    // 1. Encoder Rotation
    encStep = 0;
    int curEncA = digitalRead(Pins::ENC_A);
    int curEncB = digitalRead(Pins::ENC_B);
    
    if (curEncA != lastEncA) {
        if (curEncA == LOW) {
            if (curEncB == HIGH) encStep = 1;
            else encStep = -1;
        }
    }
    lastEncA = curEncA;
    lastEncB = curEncB;

    // 2. Encoder Button
    encPressed = false;
    bool curBtn = digitalRead(Pins::ENC_BTN);
    unsigned long now = millis();
    
    if (curBtn == LOW && lastEncBtnState == HIGH) {
        if (now - lastEncBtnMs > 150) {
            encPressed = true;
            lastEncBtnMs = now;
        }
    }
    lastEncBtnState = curBtn;

    // 3. Back Button (Key0)
    backPressed = false;
    bool curKey0 = digitalRead(Pins::KEY0);
    
    if (curKey0 == LOW) {
        if (!key0Consumed && (now - lastKey0Ms > 80)) {
            backPressed = true;
            key0Consumed = true;
            lastKey0Ms = now;
        }
    } else {
        key0Consumed = false;
        lastKey0Ms = now;
    }
}