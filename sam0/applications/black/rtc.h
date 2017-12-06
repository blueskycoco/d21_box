#ifndef _RTC_H
#define _RTC_H
bool set_rtc_time();
void get_rtc_time(struct rtc_calendar_time *cur_time);
void init_rtc(void);
#endif
