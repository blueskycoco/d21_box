#include <asf.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "conf_bootloader.h"
#include "misc.h"
#include <calendar.h>
#if CONSOLE_OUTPUT_ENABLED
#include "conf_uart_serial.h"
#endif
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
char *add_item_str(char *old,char *id,char *text)
{
	cJSON *root;
	char *out;
	if(old!=NULL)
		root=cJSON_Parse(old);
	else
		root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, id, cJSON_CreateString(text));
	out=cJSON_PrintUnformatted(root);	
	cJSON_Delete(root);
	if(old)
		free(old);
	return out;
}

char *add_item_number(char *old,char *id, double text)
{
	cJSON *root;
	char *out;
	if(old!=NULL)
		root=cJSON_Parse(old);
	else
		root=cJSON_CreateObject();	
	cJSON_AddItemToObject(root, id, cJSON_CreateNumber(text));
	out=cJSON_PrintUnformatted(root);	
	cJSON_Delete(root);
	if(old)
		free(old);
	return out;
}
char *doit(char *text,char *item_str)
{
	char *out=NULL;cJSON *item_json;

	item_json=cJSON_Parse(text);
	if (!item_json) {printf("Error data before: [%s]\n",cJSON_GetErrorPtr());}
	else
	{
		cJSON *data;
		data=cJSON_GetObjectItem(item_json,item_str);
		if(data)
		{
			int nLen = strlen(data->valuestring);
			out=(char *)malloc(nLen+1);
			memset(out,'\0',nLen+1);
			memcpy(out,data->valuestring,nLen);
		}
		cJSON_Delete(item_json);	
	}
	return out;
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