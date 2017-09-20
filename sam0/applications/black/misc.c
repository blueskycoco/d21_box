#include <asf.h>
#include <stdlib.h>
#include <string.h>
//#include "cJSON.h"
#include "conf_bootloader.h"
#include "misc.h"
#include <calendar.h>
#if CONSOLE_OUTPUT_ENABLED
#include "conf_uart_serial.h"
#endif

#define DATA 	"data"
#define TYPE 	"type"
#define XT 		"bloodSugar"
#define TS		"actionTime"
#define GID		"gid"
#define DID		"device_id"
#define JSON "{\"data\": {\"type\": %d,\"bloodSugar\": %d.%d,\"actionTime\": %d,\
			 "gid\": %d},\"deviceId\": \"%s\"}"
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
void build_json(char *out, char type, int *bloodSugar, int actionTime, int gid, 
				char *device_id)
{
#if 1
	sprintf(out, JSON,type,bloodSugar[0],bloodSugar[1],actionTime,gid,device_id);
#else
	cJSON *root;
	char *out = NULL;

	if (old != NULL)
	{
		root = cJSON_Parse(old);
		cJSON *array=cJSON_GetObjectItem(root,DATA);	
		cJSON *item = cJSON_CreateObject();
		cJSON_AddItemToObject(item, TYPE, cJSON_CreateBool(type));
		cJSON_AddItemToObject(item, XT, cJSON_CreateNumber(bloodSugar));
		cJSON_AddItemToObject(item, TS, cJSON_CreateNumber(actionTime));
		cJSON_AddItemToObject(item, GID, cJSON_CreateNumber(gid));
		cJSON_AddItemToArray(array, item);
		cJSON_ReplaceItemInObject(root,DATA,array);
		out=cJSON_PrintUnformatted(root);	
		cJSON_Delete(item);
		cJSON_Delete(array);
		cJSON_Delete(root);
		if(old)
			free(old);
	}
	else
	{
		root = cJSON_CreateObject();
		cJSON_AddItemToObject(root, DID, cJSON_CreateString(device_id));
		cJSON *array = cJSON_CreateArray();
		cJSON *item = cJSON_CreateObject();
		cJSON_AddItemToObject(item, TYPE, cJSON_CreateBool(type));
		cJSON_AddItemToObject(item, XT, cJSON_CreateNumber(bloodSugar));
		cJSON_AddItemToObject(item, TS, cJSON_CreateNumber(actionTime));
		cJSON_AddItemToObject(item, GID, cJSON_CreateNumber(gid));
		cJSON_AddItemToArray(array, item);
		cJSON_AddItemToObject(root, DATA, array);
		out=cJSON_PrintUnformatted(root);	
		cJSON_Delete(item);
		cJSON_Delete(array);
		cJSON_Delete(root);
		if(old)
			free(old);
	}
	return out;
#endif
}
uint32_t date2ts(struct rtc_calendar_time date)
{
	struct calendar_date cur_date;
	cur_date.date = date.day;
	cur_date.hour = date.hour;
	cur_date.minute = date.minute;
	cur_date.month = date.month;
	cur_date.second = date.second;
	cur_date.year = date.year;
	return calendar_date_to_timestamp(&cur_date);
}
