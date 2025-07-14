#ifndef TIMEZONE_UTILS_H
#define TIMEZONE_UTILS_H

#include <Arduino.h>

class TimezoneUtils {
  public:
    static int getTimezoneOffset(const String& timezone);
};

#endif  // TIMEZONE_UTILS_H