#ifndef _MISC_H
#define _MISC_H
//void build_json(char *out, char type, int *bloodSugar, int actionTime, int gid, char *device_id);
char *build_json(char *old, char type, int8_t bloodSugar, int actionTime, int gid, 
				char *device_id);
uint32_t date2ts(struct rtc_calendar_time date);
bool do_it(uint8_t *in, uint8_t **time);
#if CONSOLE_OUTPUT_ENABLED
void console_init(void);
#endif
#endif
