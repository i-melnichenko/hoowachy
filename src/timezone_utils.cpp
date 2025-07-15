#include "timezone_utils.h"
#include "logger.h"
#include <time.h>

// Helper function to calculate day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
// Using Zeller's congruence formula for Gregorian calendar
static int calculateDayOfWeek(int year, int month, int day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    int k = year % 100;
    int j = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + k + k/4 + j/4 - 2*j) % 7;
    // Convert to Sunday=0 format
    return (h + 5) % 7;
}

// Helper function to find last Sunday of a month
static int findLastSunday(int year, int month) {
    // Get days in month
    int daysInMonth;
    if (month == 2) {
        daysInMonth = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        daysInMonth = 30;
    } else {
        daysInMonth = 31;
    }
    
    // Start from last day and work backwards to find Sunday
    for (int day = daysInMonth; day >= 1; day--) {
        if (calculateDayOfWeek(year, month, day) == 0) { // Sunday
            return day;
        }
    }
    return 31; // Fallback (should not happen)
}

// Helper function to determine if it's daylight saving time for European timezones
// Uses UTC time and applies standard timezone offset to determine local time for DST calculation
bool TimezoneUtils::isDaylightSavingTimeForTimezone(const String& timezone) {
    // Get current UTC time
    time_t utcTime = time(nullptr);
    if (utcTime == -1) {
        LOG_INFO("TimezoneUtils: Could not get UTC time, defaulting to standard time");
        return false;  // Default to standard time if we can't get time
    }

    // Get standard (winter) timezone offset in seconds
    int standardOffset = 0;
    if (timezone == "CET" || timezone == "Europe/Berlin") {
        standardOffset = 1 * 3600;  // CET is UTC+1
    } else if (timezone == "EET" || timezone == "Europe/Kiev") {
        standardOffset = 2 * 3600;  // EET is UTC+2  
    } else if (timezone == "BST" || timezone == "Europe/London") {
        standardOffset = 0;  // GMT is UTC+0
    } else {
        // For non-European timezones, no DST calculation needed
        return false;
    }

    // Apply standard offset to get local time
    time_t localTime = utcTime + standardOffset;
    struct tm* timeinfo = gmtime(&localTime);
    if (!timeinfo) {
        LOG_INFO("TimezoneUtils: Could not convert time, defaulting to standard time");
        return false;
    }

    int year = timeinfo->tm_year + 1900;
    int month = timeinfo->tm_mon + 1;  // tm_mon is 0-based
    int day = timeinfo->tm_mday;

    // DST runs from last Sunday in March to last Sunday in October

    // Before March or after October - definitely standard time
    if (month < 3 || month > 10) {
        return false;
    }

    // April to September - definitely daylight saving time
    if (month > 3 && month < 10) {
        return true;
    }

    // March - check if we're past the last Sunday
    if (month == 3) {
        int lastSunday = findLastSunday(year, 3);
        return day >= lastSunday;
    }

    // October - check if we're before the last Sunday
    if (month == 10) {
        int lastSunday = findLastSunday(year, 10);
        return day < lastSunday;
    }

    return false;  // Default to standard time
}

// Convert timezone string to GMT offset in seconds
int TimezoneUtils::getTimezoneOffset(const String& timezone) {
    // Common timezone mappings (GMT offset in seconds)
    if (timezone == "UTC" || timezone == "GMT") return 0;

    // Europe - with DST support using proper UTC-based calculation
    if (timezone == "CET" || timezone == "Europe/Berlin") {
        return isDaylightSavingTimeForTimezone(timezone) ? 2 * 3600 : 1 * 3600;  // CEST vs CET
    }
    if (timezone == "EET" || timezone == "Europe/Kiev") {
        return isDaylightSavingTimeForTimezone(timezone) ? 3 * 3600 : 2 * 3600;  // EEST vs EET
    }
    if (timezone == "BST" || timezone == "Europe/London") {
        return isDaylightSavingTimeForTimezone(timezone) ? 1 * 3600 : 0;  // BST vs GMT
    }

    // America - static offsets (no DST calculation for now)
    if (timezone == "EST" || timezone == "America/New_York") return -5 * 3600;
    if (timezone == "CST" || timezone == "America/Chicago") return -6 * 3600;
    if (timezone == "MST" || timezone == "America/Denver") return -7 * 3600;
    if (timezone == "PST" || timezone == "America/Los_Angeles") return -8 * 3600;

    // Asia - static offsets
    if (timezone == "JST" || timezone == "Asia/Tokyo") return 9 * 3600;
    if (timezone == "CST" || timezone == "Asia/Shanghai") return 8 * 3600;
    if (timezone == "IST" || timezone == "Asia/Kolkata") return 5 * 3600 + 30 * 60;

    // Try to parse as GMT+/-X format
    if (timezone.startsWith("GMT+") || timezone.startsWith("UTC+")) {
        int hours = timezone.substring(4).toInt();
        return hours * 3600;
    }
    if (timezone.startsWith("GMT-") || timezone.startsWith("UTC-")) {
        int hours = timezone.substring(4).toInt();
        return -hours * 3600;
    }

    // Default to UTC if timezone is not recognized
    LOG_INFOF("Warning: Unknown timezone '%s', defaulting to UTC\n", timezone.c_str());
    return 0;
}