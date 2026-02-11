# Installation for Windows

## Install Arduino IDE
If you already have installed Arduino IDE, you can skip this
1.  Go to https://www.arduino.cc/en/software/ and download the installer.
1.  Use the installer to install the IDE
1.  Launch the IDE


## Install All Dependencies

1.  Go to the **Boards Manager** tab in the IDE and install the following libraries with the correct version number:
    * **esp32** by Espressif Systems 2.0.17
    * Select new **Simulation Project**
    * Name it `world_Sim`
    * Instead of **blueprint** select **C++**
    * Create the new world. If you run into a Visual Studio environment mismatch, ask your preferred LLM for help. You might need to install the right (legacy) compiler.
1.  Go to the **Library Manager** tab in the IDE and install the following libraries with the correct version number:
    * **Adafruit GFX Library** by Adafruit 1.12.4
    * **Adafruit ST7735 and ST7789 Library** by Adafruit 1.11.0
    * **Adafruit AHTX0** by Adafruit 2.0.5
    * **ENS160 – Adafruit Fork** by Adafruit 3.0.1
    * **WiFiManager** by tzapu 2.0.17
1. Select the correct board. Make sure it is the **ESP32C3 Dev Module**