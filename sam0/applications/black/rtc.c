#include <asf.h>
struct rtc_module rtc_instance;

void count_next_one_hour(struct rtc_calendar_time *cur_time, 
		struct rtc_calendar_time *next_time)
{
	next_time->hour 	= cur_time->hour + 1;
	next_time->hour 	= next_time->hour % 24;
	next_time->second 	= cur_time->second;
	next_time->year 	= cur_time->year;
	next_time->month 	= cur_time->month;
	next_time->day 		= cur_time->day;
	next_time->minute 	= cur_time->minute;
}
void rtc_match_callback(void)
{
	struct rtc_calendar_time cur_time;
	struct rtc_calendar_alarm_time alarm;
	rtc_calendar_get_time(&rtc_instance, &cur_time);
	count_next_one_hour(&cur_time, &next_time);

	alarm.time.year      = next_time.year;
	alarm.time.month     = next_time.month;
	alarm.time.day       = next_time.day;
	alarm.time.hour      = next_time.hour;
	alarm.time.minute    = next_time.minute;
	alarm.time.second    = next_time.second;
	alarm.mask = RTC_CALENDAR_ALARM_MASK_HOUR;	
	rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);
}
bool set_rtc_time(struct rtc_calendar_time cur_time)
{
	static bool init = false;
	struct rtc_calendar_time next_time;
	struct rtc_calendar_alarm_time alarm;
	
	count_next_one_hour(&cur_time, &next_time);
	alarm.time.year      = next_time.year;
	alarm.time.month     = next_time.month;
	alarm.time.day       = next_time.day;
	alarm.time.hour      = next_time.hour;
	alarm.time.minute    = next_time.minute;
	alarm.time.second    = next_time.second;
	alarm.mask 			 = RTC_CALENDAR_ALARM_MASK_HOUR;

	if (!init)
	{
		struct rtc_calendar_config config_rtc_calendar;
		rtc_calendar_get_config_defaults(&config_rtc_calendar);

		config_rtc_calendar.clock_24h = true;
		config_rtc_calendar.alarm[0].time = alarm.time;
		config_rtc_calendar.alarm[0].mask = RTC_CALENDAR_ALARM_MASK_HOUR;

		rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);
		rtc_calendar_enable(&rtc_instance);
		rtc_calendar_register_callback(
				&rtc_instance, rtc_match_callback, 
				RTC_CALENDAR_CALLBACK_ALARM_0);
		rtc_calendar_enable_callback(&rtc_instance, 
				RTC_CALENDAR_CALLBACK_ALARM_0);
		init = true;
	}
	else
	{
		rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);
	}

	rtc_calendar_set_time(&rtc_instance, &cur_time);

	return true;
}
