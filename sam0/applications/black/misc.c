#include <asf.h>
#include <stdlib.h>
#include <string.h>
#include "conf_bootloader.h"
#include "misc.h"
#include <rtc_calendar.h>
#include <time.h>

#if CONSOLE_OUTPUT_ENABLED
#include "conf_uart_serial.h"
#endif

#define DATA 	"data"
#define TYPE 	"type"
#define XT 		"bloodSugar"
#define TS		"actionTime"
#define GID		"gid"
#define DID		"deviceId"
#define JSON "{\"data\": [{\"type\": %d,\"bloodSugar\": %d,\"actionTime\": %d,\"gid\": %d}], \"deviceId\": \"%s\"}"
#if CONSOLE_OUTPUT_ENABLED
/**
 * \brief Initializes the console output
 */
void console_init(void)
{
	struct usart_config uart_conf;
	static struct usart_module cdc_uart_module;

	usart_get_config_defaults(&uart_conf);
	uart_conf.mux_setting = CONF_STDIO_MUX_SETTING;
	uart_conf.pinmux_pad0 = CONF_STDIO_PINMUX_PAD0;
	uart_conf.pinmux_pad1 = CONF_STDIO_PINMUX_PAD1;
	uart_conf.pinmux_pad2 = CONF_STDIO_PINMUX_PAD2;
	uart_conf.pinmux_pad3 = CONF_STDIO_PINMUX_PAD3;
	uart_conf.baudrate    = CONF_STDIO_BAUDRATE;

	stdio_serial_init(&cdc_uart_module, CONF_STDIO_USART_MODULE, &uart_conf);
	usart_enable(&cdc_uart_module);
}
#endif
char *build_json(char *old, char type, uint32_t bloodSugar, int actionTime, int gid, 
		char *device_id)
{
	sprintf(old, JSON,type,bloodSugar,actionTime,gid,device_id);
	return old;
}
bool do_it(uint8_t *in, uint32_t *time)
{
	return false;
}
void ts2date(uint32_t time, struct rtc_calendar_time *date_out)
{
	char res[32] = {0};
	char tmp[5] = {0};
	struct tm lt;
	localtime_r(&time, &lt);
	strftime(res, sizeof(res), "%Y-%m-%d %H:%M:%S", &lt);
	memcpy(tmp, res+5, 2);
	date_out->month = atoi(tmp);
	memcpy(tmp, res+8, 2);
	date_out->day = atoi(tmp);
	memcpy(tmp, res+11, 2);
	date_out->hour= atoi(tmp) + 8;
	memcpy(tmp, res+14, 2);
	date_out->minute= atoi(tmp);
	memcpy(tmp, res+17, 2);
	date_out->second= atoi(tmp);
	memcpy(tmp, res, 4);
	date_out->year= atoi(tmp);
}
/*uint32_t date2ts(struct rtc_calendar_time date)
  {
  struct calendar_date cur_date;
  cur_date.date = date.day;
  cur_date.hour = date.hour;
  cur_date.minute = date.minute;
  cur_date.month = date.month;
  cur_date.second = date.second;
  cur_date.year = date.year;
  return calendar_date_to_timestamp(&cur_date);
  }*/
void toHex(uint8_t *input, uint32_t len, uint8_t *output)
{
	int ix=0,iy=0;
	for (ix=0; ix<len; ix++) {
		output[iy] = (input[ix]&0xf0) >>4;
		if(output[iy]>=10 && output[iy]<=15)
		{
			output[iy] = output[iy] + 'a' - 10;
		}
		else
		{
			output[iy] = output[iy] + '0';
		}
		iy++;
		output[iy] = input[ix]&0x0f;
		if(output[iy] >= 10 && output[iy] <= 15)
		{
			output[iy] = output[iy] + 'a' - 10;
		}
		else
		{
			output[iy] = output[iy] + '0';
		}
		iy++;
	}
}
