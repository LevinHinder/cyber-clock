#ifndef MODE_H
#define MODE_H

#include <Arduino.h>

class Mode {
public:
    virtual ~Mode() {}
    virtual void enter() = 0;
    virtual void loop() = 0; 
    // Optional: virtual void exit() {} 
};

#endif