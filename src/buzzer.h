#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "logger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "event_manager.h"
#include "pins.h"

class Buzzer {
  public:
    static void Setup();
    static void Run();

  private:
    enum class SoundType { NONE, BUTTON, ALARM_ON, ALARM_OFF };

    static uint8_t volume;
    static SoundType currentSound;
    static uint8_t soundStep;

    static void on_button_press(const ButtonShortPressEvent& event);
    static void on_alarm_on(const CriticalAlarmEvent& event);
    static void on_alarm_off(const CriticalAlarmOffEvent& event);

    static void beep(uint32_t duration_ms, uint32_t frequency_hz);
};

#endif  // BUZZER_H