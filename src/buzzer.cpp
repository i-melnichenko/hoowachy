#include "buzzer.h"
#include "logger.h"

// Static member variable definitions
uint8_t Buzzer::volume = 200;
Buzzer::SoundType Buzzer::currentSound = Buzzer::SoundType::NONE;
uint8_t Buzzer::soundStep = 0;

void Buzzer::Setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    // digitalWrite(BUZZER_PIN, LOW);

    delay(1000);

    ledcSetup(0, 1000, 8);
    ledcAttachPin(BUZZER_PIN, 0);

    // Subscribe to new event classes
    EventManager::Subscribe<ButtonShortPressEvent>(on_button_press);
    EventManager::Subscribe<CriticalAlarmEvent>(on_alarm_on);
    EventManager::Subscribe<CriticalAlarmOffEvent>(on_alarm_off);
}

void Buzzer::Run() {
    LOG_DEBUG("Buzzer Run");
    while (true) {
        if (currentSound != SoundType::NONE) {
            switch (currentSound) {
                case SoundType::BUTTON:
                    beep(50, 1000);
                    currentSound = SoundType::NONE;
                    break;

                case SoundType::ALARM_ON:
                    if (soundStep < 3) {
                        beep(100, 800);
                        soundStep++;
                    } else {
                        currentSound = SoundType::NONE;
                        soundStep = 0;
                    }
                    break;

                case SoundType::ALARM_OFF:
                    beep(500, 200);
                    currentSound = SoundType::NONE;
                    break;

                default:
                    break;
            }
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void Buzzer::on_button_press(const ButtonShortPressEvent& event) {
    LOG_DEBUGF("Button press - ID: %d, Duration: %lu ms", event.button_id, event.press_duration_ms);
    currentSound = SoundType::BUTTON;
}

void Buzzer::on_alarm_on(const CriticalAlarmEvent& event) {
    LOG_WARNINGF("Alarm ON - Message: %s, Severity: %d", event.alarm_message, event.severity_level);
    currentSound = SoundType::ALARM_ON;
    soundStep = 0;
}

void Buzzer::on_alarm_off(const CriticalAlarmOffEvent& event) {
    LOG_INFOF("Alarm OFF - Reason: %s", event.reason);
    currentSound = SoundType::ALARM_OFF;
}

void Buzzer::beep(uint32_t duration_ms, uint32_t frequency_hz) {
    LOG_DEBUGF("Beep: %d, %d", duration_ms, frequency_hz);
    ledcWriteTone(0, frequency_hz);
    ledcWrite(0, Buzzer::volume);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledcWrite(0, 0);
}
