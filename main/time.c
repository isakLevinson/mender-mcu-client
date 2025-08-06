
#define DEF_DBG_MODULE	DBG_MODULE_TIME

#include <sys_def.h>
#include "dbgMenus.h"
#include "dbgPrint.h"
#include "parseArgs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_system.h"

#include "time.h"
#include "main.h"

static struct {
	int64_t	offset64;
	int64_t	lastUpdated;
} g_timerDb;

int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

// Number of days in each month
static const int days_in_month[] = {
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
};

int64_t TIME_mktime(tm_t *t)
{
    static const int SECONDS_PER_MINUTE = 60;
    static const int SECONDS_PER_HOUR = 3600;
    static const int SECONDS_PER_DAY = 86400;

    uint64_t days = 0;

    // Years since 1970
    for (int year = 1970; year < t->year; year++) {
        days += is_leap(year) ? 366 : 365;
    }

    // Months in current year
    for (int month = 1; month < t->mon; month++) {
        days += days_in_month[month];
        if (month == 1 && is_leap(t->year)) {
            days += 1;  // February in leap year
        }
    }

    // Days in current month
    days += t->day - 1;

    // Convert to seconds
    uint64_t seconds = days * SECONDS_PER_DAY;
    seconds += t->hour * SECONDS_PER_HOUR;
    seconds += t->min * SECONDS_PER_MINUTE;
    seconds += t->sec;

    return seconds;
}

int64_t TIME_set64(int64_t time)
{
	int64_t t;

	t = esp_timer_get_time();

	g_timerDb.offset64		= t - time;
	g_timerDb.lastUpdated	= time;

	return t;
}

void TIME_getUpdateTime(int64_t* o_pTime)
{
	*o_pTime = g_timerDb.lastUpdated;
}

void TIME_get64(int64_t* o_pTime)
{
	int64_t t;

	t = esp_timer_get_time();
	*o_pTime = t - g_timerDb.offset64;
}

int64_t TIME_getSec(void)
{
	int64_t t;
	TIME_get64(&t);

	return t / 1000000;
}

int32_t TIME_get32(void)
{
	int64_t t;

	t = esp_timer_get_time();

	return (int32_t)((t / 1000) & 0xffffffff);
}

void TIME_strftime(int64_t epoch, char* format, char* str)
{
	int year = 1970;
	int month = 0;
	int day = 0;
	int hour = 0;
	int min = 0;
	int sec = 0;
	uint16_t i = 0;

	epoch += (TIME_ZONE_HOURS * 3600);

	// Break into days and remaining seconds
	uint32_t days = epoch / 86400;
	uint32_t rem = epoch % 86400;

	// Time
	hour = rem / 3600;
	rem %= 3600;
	min = rem / 60;
	sec = rem % 60;

	// Date
	while (1) {
		int days_in_year = 365;
		if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
			days_in_year = 366;
		}
		if (days < days_in_year) {
			break;
		}
		days -= days_in_year;
		year++;
	}

	int month_lengths[12];
	for (int i = 0; i < 12; i++) {
		month_lengths[i] = days_in_month[i];
	}

	// Adjust February for leap years
	if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
		month_lengths[1] = 29;
	}

	for (month = 0; month < 12; month++) {
		if (days < month_lengths[month]) {
			break;
		}
		days -= month_lengths[month];
	}

	day = days + 1;  // day of month is 1-based

	TRACE("time: %04d-%02d-%02d %02d:%02d:%02d\n", year, month + 1, day, hour, min, sec);

	for (; *format; format++) {
		if (*format == '%') {
			format++;
			if (!*format) {
				break;
			}

			char temp[5];
			switch (*format) {
				case 'Y':
					snprintf(temp, sizeof(temp), "%04d", year);
					break;
				case 'm':
					snprintf(temp, sizeof(temp), "%02d", month + 1);
					break;
				case 'd':
					snprintf(temp, sizeof(temp), "%02d", day);
					break;
				case 'H':
					snprintf(temp, sizeof(temp), "%02d", hour);
					break;
				case 'M':
					snprintf(temp, sizeof(temp), "%02d", min);
					break;
				case 'S':
					snprintf(temp, sizeof(temp), "%02d", sec);
					break;
				default:
					temp[0] = '%';
					temp[1] = *format;
					temp[2] = '\0';
					break;
			}
			for (char* p = temp; *p; ++p) {
				str[i++] = *p;
			}
		} else {
			str[i++] = *format;
		}
	}
	str[i] = '\0';
}
