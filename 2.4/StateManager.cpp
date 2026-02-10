#include "StateManager.h"

StateManager State;

void StateManager::switchMode(Mode *newMode) {
    if (nextMode != nullptr) {
        delete nextMode;
    }
    nextMode = newMode;
}

void StateManager::update() {
    if (nextMode != nullptr) {
        if (currentMode != nullptr) {
            delete currentMode;
        }
        currentMode = nextMode;
        nextMode = nullptr;
        currentMode->enter();
    }

    if (currentMode != nullptr) {
        currentMode->loop();
    }
}