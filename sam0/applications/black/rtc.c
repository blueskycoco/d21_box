#include <asf.h>
#include <stdbool.h>
#include "rtc.h"
struct rtc_module rtc_instance;
struct rtc_calendar_alarm_time alarm;

static void rtc_match_callback(void)
{
	alarm.time.second += 10;
	alarm.time.second = alarm.time.second % 60;
	alarm.mask = RTC_CALENDAR_ALARM_MASK_SEC;	
	rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);
}
bool set_rtc_time(struct rtc_calendar_time cur_time)
{	
	alarm.time.year      = cur_time.year;
	alarm.time.month     = cur_time.month;
	alarm.time.day       = cur_time.day;
	alarm.time.hour      = cur_time.hour + 1;
	alarm.time.hour 	 = alarm.time.hour % 24;
	alarm.time.minute    = cur_time.minute;
	alarm.time.second    = cur_time.second;
	alarm.mask 			 = RTC_CALENDAR_ALARM_MASK_SEC;
	
	rtc_calendar_set_alarm(&rtc_instance, &alarm, RTC_CALENDAR_ALARM_0);
	rtc_calendar_set_time(&rtc_instance, &cur_time);

	return true;
}
void init_rtc(void)
{
	struct rtc_calendar_config config_rtc_calendar;
	rtc_calendar_get_config_defaults(&config_rtc_calendar);
	
	config_rtc_calendar.clock_24h = true;
	config_rtc_calendar.alarm[0].time = alarm.time;
	config_rtc_calendar.alarm[0].mask = RTC_CALENDAR_ALARM_MASK_SEC;
	
	rtc_calendar_init(&rtc_instance, RTC, &config_rtc_calendar);
	rtc_calendar_enable(&rtc_instance);
	rtc_calendar_register_callback(
			&rtc_instance, rtc_match_callback, 
			RTC_CALENDAR_CALLBACK_ALARM_0);
	rtc_calendar_enable_callback(&rtc_instance, 
			RTC_CALENDAR_CALLBACK_ALARM_0);
}
void get_rtc_time(struct rtc_calendar_time *cur_time)
{
	rtc_calendar_get_time(&rtc_instance, cur_time);
	printf("cur time \r\n");
	printf("year  : %d\r\n",cur_time->year);
	printf("month : %d\r\n",cur_time->month);
	printf("day   : %d\r\n",cur_time->day);
	printf("hour  : %d\r\n",cur_time->hour);
	printf("minute: %d\r\n",cur_time->minute);
	printf("second: %d\r\n",cur_time->second);
}
