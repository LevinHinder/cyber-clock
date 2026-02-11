#include "InputManager.h"

// Global ISR variables
volatile bool isrTriggered = false;
volatile unsigned long lastIsrTime = 0;

// The Interrupt Function
void IRAM_ATTR ISR_Key0() {
    unsigned long now = millis();
    if (now - lastIsrTime > 250) { // 250ms hardware debounce
        isrTriggered = true;
        lastIsrTime = now;
    }
}

void InputManager::begin() {
    pinMode(Pins::ENC_A, INPUT_PULLUP);
    pinMode(Pins::ENC_B, INPUT_PULLUP);
    pinMode(Pins::ENC_BTN, INPUT_PULLUP);
    
    pinMode(Pins::KEY0, INPUT_PULLUP);
    // Attach the interrupt to catch the press INSTANTLY
    attachInterrupt(digitalPinToInterrupt(Pins::KEY0), ISR_Key0, FALLING);
}

void InputManager::update() {
    unsigned long now = millis();

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
    if (curBtn == LOW && lastEncBtnState == HIGH) {
        if (now - lastEncBtnMs > 150) {
            encPressed = true;
            lastEncBtnMs = now;
        }
    }
    lastEncBtnState = curBtn;

    // 3. Back Button (ISR Handover)
    // We check if the ISR fired since the last update
    if (isrTriggered) {
        backPressed = true;
        isrTriggered = false; // Reset the ISR flag so we don't trigger twice
        Serial.println("Key0 ISR Triggered"); // Debug
    } else {
        backPressed = false;
    }
}