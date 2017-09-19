#ifndef _MISC_H
#define _MISC_H
#if 0
void build_json(char *out, char type, int *bloodSugar, int actionTime, int gid, char *device_id);
#else
char *build_json(char *old, char type, float bloodSugar, int actionTime, int gid, char *device_id);
#endif
uint32_t date2ts(struct rtc_calendar_time date);
#if CONSOLE_OUTPUT_ENABLED
void console_init(void);
#endif
#endif
