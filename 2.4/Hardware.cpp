#include "Hardware.h"
#include "Graphics.h"

void initHardware() {
    Input.begin();

    ledcSetup(PWM::CH_LED, 5000, 8);
    ledcAttachPin(Pins::LED, PWM::CH_LED);
    ledcSetup(PWM::CH_BUZZ, 2000, 8);
    ledcAttachPin(Pins::BUZZ, PWM::CH_BUZZ);

    for (int i = 0; i < EnvData::HIST_LEN; i++) {
        env.hTemp[i] = 0;
        env.hHum[i] = 0;
        env.hTVOC[i] = 0;
        env.hCO2[i] = 0;
    }
}

void setLedState(bool on) {
    if (on) {
        int duty = map(settings.ledBrightness, 0, 100, 0, 255);
        ledcWrite(PWM::CH_LED, duty);
    } else {
        ledcWrite(PWM::CH_LED, 0);
    }
}

void playSystemTone(unsigned int frequency, unsigned long durationMs) {
    if (settings.speakerVol == 0) {
        ledcWrite(PWM::CH_BUZZ, 0);
        return;
    }
    ledcSetup(PWM::CH_BUZZ, frequency, 8);
    ledcAttachPin(Pins::BUZZ, PWM::CH_BUZZ);
    int duty = map(settings.speakerVol, 0, 100, 0, 128);
    ledcWrite(PWM::CH_BUZZ, duty);
    if (durationMs > 0) {
        delay(durationMs);
        stopSystemTone();
    }
}

void stopSystemTone() { ledcWrite(PWM::CH_BUZZ, 0); }

void recordHistory(float t, float h, uint16_t tvoc, uint16_t eco2) {
    env.hTemp[env.head] = (uint16_t)(t * 10);
    env.hHum[env.head] = (uint16_t)h;
    env.hTVOC[env.head] = tvoc;
    env.hCO2[env.head] = eco2;
    env.head = (env.head + 1) % EnvData::HIST_LEN;
    if (ui.currentMode == MODE_CLOCK)
        drawHistoryGraph();
}

void updateEnvSensors(bool force) {
    unsigned long now = millis();
    if (force || (now - env.lastRead) >= 1000) {
        env.lastRead = now;
        sensors_event_t hum, temp;
        if (aht.getEvent(&hum, &temp)) {
            env.temp = temp.temperature;
            env.hum = hum.relative_humidity;
        }
        ens160.set_envdata(env.temp, env.hum);
        ens160.measure();
        uint16_t newTVOC = ens160.getTVOC();
        uint16_t newCO2 = ens160.geteCO2();
        if (newTVOC != 0xFFFF)
            env.tvoc = newTVOC;
        if (newCO2 != 0xFFFF)
            env.eco2 = newCO2;
    }
    unsigned long interval =
        (settings.graphDuration * 60000UL) / EnvData::HIST_LEN;
    if (interval < 1000)
        interval = 1000;
    if (now - env.lastHistAdd >= interval) {
        env.lastHistAdd = now;
        recordHistory(env.temp, env.hum, env.tvoc, env.eco2);
    }
}

void loadSettings() {
    prefs.begin("cyber", true);
    settings.ledBrightness = prefs.getInt("led_b", 100);
    settings.speakerVol = prefs.getInt("spk_v", 100);
    settings.graphDuration = prefs.getInt("gr_dur", 5);
    settings.alarmHour = prefs.getInt("alm_h", 7);
    settings.alarmMinute = prefs.getInt("alm_m", 0);
    settings.alarmEnabled = prefs.getBool("alm_e", false);
    settings.pomoWorkMin = prefs.getInt("p_work", 25);
    settings.pomoShortMin = prefs.getInt("p_short", 5);
    settings.pomoLongMin = prefs.getInt("p_long", 15);
    settings.pomoCycles = prefs.getInt("p_cycl", 4);
    prefs.end();
}

void saveSettings() {
    prefs.begin("cyber", false);
    prefs.putInt("led_b", settings.ledBrightness);
    prefs.putInt("spk_v", settings.speakerVol);
    prefs.putInt("gr_dur", settings.graphDuration);
    prefs.putInt("alm_h", settings.alarmHour);
    prefs.putInt("alm_m", settings.alarmMinute);
    prefs.putBool("alm_e", settings.alarmEnabled);
    prefs.putInt("p_work", settings.pomoWorkMin);
    prefs.putInt("p_short", settings.pomoShortMin);
    prefs.putInt("p_long", settings.pomoLongMin);
    prefs.putInt("p_cycl", settings.pomoCycles);
    prefs.end();
}

void syncTime() {
    configTime(0, 0, Net::NTP_SERVER);
    setenv("TZ", Net::TIME_ZONE, 1);
    tzset();
}

String getTimeStr(char type) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return "--";
    char buf[8];
    if (type == 'H')
        strftime(buf, sizeof(buf), "%H", &timeinfo);
    else if (type == 'M')
        strftime(buf, sizeof(buf), "%M", &timeinfo);
    else if (type == 'S')
        strftime(buf, sizeof(buf), "%S", &timeinfo);
    return String(buf);
}

void updateAlertStateAndLED() {
    if (ui.currentMode == MODE_WIFI_SETUP)
        return;
    if (ui.alarmRinging)
        ui.currentAlert = ALERT_ALARM;
    else if (env.eco2 > 1800)
        ui.currentAlert = ALERT_CO2;
    else
        ui.currentAlert = ALERT_NONE;

    unsigned long now = millis();
    if (ui.currentAlert == ALERT_NONE) {
        if (ui.currentMode != MODE_SETTINGS &&
            ui.currentMode != MODE_SETTINGS_EDIT) {
            setLedState(false);
        }
        ui.ledState = false;
    } else {
        unsigned long interval = (ui.currentAlert == ALERT_ALARM) ? 120 : 250;
        if (now - ui.lastLedToggleMs > interval) {
            ui.lastLedToggleMs = now;
            ui.ledState = !ui.ledState;
            setLedState(ui.ledState);
        }
    }
    if (ui.currentAlert == ALERT_CO2) {
        if (now - ui.lastCo2BlinkMs > 350) {
            ui.lastCo2BlinkMs = now;
            ui.co2BlinkOn = !ui.co2BlinkOn;
            playSystemTone(1800, 80);
        }
    }
}