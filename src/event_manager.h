#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <Arduino.h>
#include "logger.h"
#include <functional>
#include <memory>
#include <typeinfo>
#include <vector>

// Base class for all events
class Event {
  public:
    virtual ~Event() = default;
    virtual const char* GetTypeName() const = 0;
};

// Button events
class ButtonShortPressEvent : public Event {
  public:
    ButtonShortPressEvent(int button_id, unsigned long press_duration_ms)
        : button_id(button_id), press_duration_ms(press_duration_ms) {}

    int button_id;
    unsigned long press_duration_ms;

    const char* GetTypeName() const override { return "ButtonShortPressEvent"; }
};

class ButtonLongPressEvent : public Event {
  public:
    ButtonLongPressEvent(int button_id, unsigned long press_duration_ms)
        : button_id(button_id), press_duration_ms(press_duration_ms) {}

    int button_id;
    unsigned long press_duration_ms;

    const char* GetTypeName() const override { return "ButtonLongPressEvent"; }
};

// Alarm events
class CriticalAlarmEvent : public Event {
  public:
    CriticalAlarmEvent(const char* alarm_message, int severity_level)
        : alarm_message(alarm_message), severity_level(severity_level) {}

    const char* alarm_message;
    int severity_level;

    const char* GetTypeName() const override { return "CriticalAlarmEvent"; }
};

class CriticalAlarmOffEvent : public Event {
  public:
    CriticalAlarmOffEvent(const char* reason) : reason(reason) {}

    const char* reason;

    const char* GetTypeName() const override { return "CriticalAlarmOffEvent"; }
};

// Terminal event - unified event for all terminal updates
class TerminalEvent : public Event {
  public:
    enum class State { PROCESSING, SUCCESS, FAILURE };

    TerminalEvent(int8_t step, const String& group, const String& info, State state)
        : step(step), group(group), info(info), state(state) {}

    TerminalEvent(int8_t step, const String& group, const String& info, State state, const String& extra)
        : step(step), group(group), info(info), state(state), extra(extra) {}

    int8_t step;
    String group;
    String info;
    State state;
    String extra = "";  // Additional information for complex events

    const char* GetTypeName() const override { return "TerminalEvent"; }
};

class EventManager {
  public:
    template <typename T>
    using EventCallback = std::function<void(const T&)>;

    template <typename T>
    static void Subscribe(EventCallback<T> callback) {
        static_assert(std::is_base_of<Event, T>::value, "T must be derived from Event");
        GetCallbacks<T>().push_back(callback);
    }

    template <typename T>
    static void Unsubscribe(EventCallback<T> callback) {
        static_assert(std::is_base_of<Event, T>::value, "T must be derived from Event");
        auto& callbacks = GetCallbacks<T>();
        // Note: Function comparison is complex, for now we'll clear all callbacks
        // In a real implementation, you'd need a more sophisticated approach
        callbacks.clear();
    }

    template <typename T>
    static void Emit(const T& event) {
        static_assert(std::is_base_of<Event, T>::value, "T must be derived from Event");

        const auto& callbacks = GetCallbacks<T>();
        for (const auto& callback : callbacks) {
            callback(event);
        }
    }

  private:
    template <typename T>
    static std::vector<EventCallback<T>>& GetCallbacks() {
        static std::vector<EventCallback<T>> callbacks;
        return callbacks;
    }
};

#endif  // EVENT_MANAGER_H