#include "button.h"
#include "logger.h"
#include "config.h"
#include "event_manager.h"

void Button::Setup() { pinMode(BUTTON_PIN, INPUT_PULLUP); }

void Button::Run() {
    LOG_DEBUG("Button Run");
    while (true) {
        static uint32_t pressStartTime = 0;
        static bool lastState = LOW;

        bool currentState = digitalRead(BUTTON_PIN) == LOW;

        if (currentState == HIGH && lastState == LOW) {
            pressStartTime = millis();
            lastState = HIGH;
        } else if (currentState == LOW && lastState == HIGH) {
            lastState = LOW;
            uint32_t pressDuration = millis() - pressStartTime;

            if (pressDuration < BUTTON_LONG_PRESS_TIME) {
                // Emit ButtonShortPressEvent with button ID and duration
                ButtonShortPressEvent event(1, pressDuration);
                EventManager::Emit(event);
            } else {
                // Emit ButtonLongPressEvent with button ID and duration
                ButtonLongPressEvent event(1, pressDuration);
                EventManager::Emit(event);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
