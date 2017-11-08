#ifndef _MISC_H
#define _MISC_H
//void build_json(char *out, char type, int *bloodSugar, int actionTime, int gid, char *device_id);
char *build_json(char *old, char type, uint32_t bloodSugar, int actionTime, int gid, 
				char *device_id);
uint32_t date2ts(struct rtc_calendar_time date);
bool do_it(uint8_t *in, uint32_t *time);
void toHex(uint8_t *input, uint32_t len, uint8_t *output);
void fromHex(uint8_t *input, uint32_t len, uint8_t *output);
//void ts2date(uint32_t time, struct calendar_date *date_out);

#if CONSOLE_OUTPUT_ENABLED
void console_init(void);
#endif
#endif
