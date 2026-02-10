#ifndef STATEMANAGER_H
#define STATEMANAGER_H

#include "Mode.h"

class StateManager {
  private:
    Mode *currentMode = nullptr;
    Mode *nextMode = nullptr;

  public:
    void switchMode(Mode *newMode);
    void update();
};

extern StateManager State;

#endif