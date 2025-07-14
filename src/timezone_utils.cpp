#include "timezone_utils.h"
#include <time.h>

// Helper function to determine if it's daylight saving time for European timezones
static bool isDaylightSavingTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;  // Default to standard time if we can't get time
    }

    int month = timeinfo.tm_mon + 1;  // tm_mon is 0-based
    int day = timeinfo.tm_mday;
    int weekday = timeinfo.tm_wday;  // 0=Sunday, 1=Monday, etc.

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
        // Find last Sunday of March
        int lastSunday = 31;  // Start from last day of March
        while (lastSunday > 0) {
            // Calculate day of week for this date
            struct tm testDate = timeinfo;
            testDate.tm_mday = lastSunday;
            testDate.tm_mon = 2;  // March is month 2 (0-based)
            mktime(&testDate);    // Normalize the date

            if (testDate.tm_wday == 0) {  // Sunday
                break;
            }
            lastSunday--;
        }
        return day >= lastSunday;
    }

    // October - check if we're before the last Sunday
    if (month == 10) {
        // Find last Sunday of October
        int lastSunday = 31;  // Start from last day of October
        while (lastSunday > 0) {
            // Calculate day of week for this date
            struct tm testDate = timeinfo;
            testDate.tm_mday = lastSunday;
            testDate.tm_mon = 9;  // October is month 9 (0-based)
            mktime(&testDate);    // Normalize the date

            if (testDate.tm_wday == 0) {  // Sunday
                break;
            }
            lastSunday--;
        }
        return day < lastSunday;
    }

    return false;  // Default to standard time
}

// Convert timezone string to GMT offset in seconds
int TimezoneUtils::getTimezoneOffset(const String& timezone) {
    // Common timezone mappings (GMT offset in seconds)
    if (timezone == "UTC" || timezone == "GMT") return 0;

    // Europe - with DST support
    if (timezone == "CET" || timezone == "Europe/Berlin") {
        return isDaylightSavingTime() ? 2 * 3600 : 1 * 3600;  // CEST vs CET
    }
    if (timezone == "EET" || timezone == "Europe/Kiev") {
        return isDaylightSavingTime() ? 3 * 3600 : 2 * 3600;  // EEST vs EET
    }
    if (timezone == "BST" || timezone == "Europe/London") {
        return isDaylightSavingTime() ? 1 * 3600 : 0;  // BST vs GMT
    }

    // America
    if (timezone == "EST" || timezone == "America/New_York") return -5 * 3600;
    if (timezone == "CST" || timezone == "America/Chicago") return -6 * 3600;
    if (timezone == "MST" || timezone == "America/Denver") return -7 * 3600;
    if (timezone == "PST" || timezone == "America/Los_Angeles") return -8 * 3600;

    // Asia
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
    Serial.printf("Warning: Unknown timezone '%s', defaulting to UTC\n", timezone.c_str());
    return 0;
}