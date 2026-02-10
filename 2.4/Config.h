#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Adafruit_ST7789.h> 

namespace Net {
    const char* const NTP_SERVER = "pool.ntp.org";
    const char* const TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3"; 
}

namespace PWM {
    constexpr int CH_LED = 0;
    constexpr int CH_BUZZ = 1;
}

namespace Pins {
    constexpr int TFT_CS   = 9;
    constexpr int TFT_DC   = 8;
    constexpr int TFT_RST  = 7;
    constexpr int TFT_MOSI = 6;
    constexpr int TFT_SCLK = 5;

    constexpr int ENC_A    = 10;
    constexpr int ENC_B    = 20;
    constexpr int ENC_BTN  = 21;
    constexpr int KEY0     = 0;

    constexpr int I2C_SDA  = 1;
    constexpr int I2C_SCL  = 2;

    constexpr int LED      = 4;
    constexpr int BUZZ     = 3;
}

namespace Screen {
    constexpr int WIDTH  = 320;
    constexpr int HEIGHT = 240;
    constexpr int CX     = WIDTH / 2;
    constexpr int CY     = HEIGHT / 2;
}

namespace Colors {
    constexpr uint16_t BG         = ST77XX_BLACK;
    constexpr uint16_t GREEN      = 0x07E0;
    constexpr uint16_t ACCENT     = 0x07FF;
    constexpr uint16_t LIGHT      = 0xFD20;
    constexpr uint16_t BLUE       = 0x07FF;
    constexpr uint16_t PINK       = 0xF81F;
    constexpr uint16_t DARK       = 0x4208;
    constexpr uint16_t GRID       = 0x2104; 
    
    constexpr uint16_t TEMP       = ST77XX_RED;
    constexpr uint16_t HUM        = BLUE;
    constexpr uint16_t TVOC       = GREEN;
    constexpr uint16_t CO2        = ST77XX_YELLOW;
}

namespace Layout {
    constexpr int GRID_L     = 10;
    constexpr int GRID_R     = 310;
    constexpr int GRID_MID_X = (GRID_L + GRID_R) / 2;
    constexpr int GRID_TOP   = 75; 
    constexpr int GRID_MID   = 107;
    constexpr int GRID_BOT   = 139;
    constexpr int LBL_TOP_Y  = GRID_TOP + 4;  
    constexpr int VAL_TOP_Y  = GRID_TOP + 14; 
    constexpr int LBL_BOT_Y  = GRID_MID + 4;  
    constexpr int VAL_BOT_Y  = GRID_MID + 14; 
}

#endif