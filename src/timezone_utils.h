#ifndef TIMEZONE_UTILS_H
#define TIMEZONE_UTILS_H

#include <Arduino.h>
#include "logger.h"

class TimezoneUtils {
  public:
    static int getTimezoneOffset(const String& timezone);
    
  private:
    // Helper function to determine DST for European timezones based on UTC time and timezone
    static bool isDaylightSavingTimeForTimezone(const String& timezone);
};

#endif  // TIMEZONE_UTILS_H