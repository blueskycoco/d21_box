#include <stdio.h>
#include <asf.h>
#include "box_usb.h"
#include "calendar.h"
#include "apollo.h"
#include "stream.h"
uint8_t cur_libre_serial_no[32] = {0};
extern bool libre_found;
uint32_t g_num = 0;
uint8_t buf[4096] = {0};
extern void upload_json(uint8_t *xt_data, uint32_t xt_len);

void submit_serial(char * serial)
{
	printf("Serial Number: %s\r\n", serial);
	memset(cur_libre_serial_no,0,32);
	strcpy(cur_libre_serial_no, serial);
}

void submit_recorder(unsigned char cate, unsigned char data, unsigned short num, unsigned int time)
{
	struct calendar_date date_out;
	calendar_timestamp_to_date(time, &date_out);
	printf("Num\t%5d\tTime\t%4d-%02d-%02d %02d:%02d:%02d\tData\t%3d\r\n", num, date_out.year,
			date_out.month, date_out.date, date_out.hour, date_out.minute, date_out.second, data);
	//printf("Num\t%5d\tTime\t%10d\tData\t%3d\r\n", num, time, data);
	if (g_num == 4095) {
		/* flush to spi flash*/
		upload_json(buf, g_num);
		g_num = 0;		
	}
	buf[g_num++] = (num>>8) & 0xff;
	buf[g_num++] = num & 0xff;
	buf[g_num++] = (time>>24) & 0xff;
	buf[g_num++] = (time>>16) & 0xff;
	buf[g_num++] = (time>>8) & 0xff;
	buf[g_num++] = (time) & 0xff;
	buf[g_num++] = (data) & 0xff;
}

void submit_date_time(unsigned char second, unsigned char minute, unsigned char hour, unsigned char day, unsigned char month, unsigned int year)
{
	printf("Date-Time is: %4d-%02d-%02d %2d:%02d:%02d\r\n", year, month, day, hour, minute, second);
}

void ui_init(void)
{
	if (!libre_found)
		return;
	apollo_byte_stream_reset();
	apollo_req_recorder_all();
	apollo_byte_stream_reset();
	apollo_req_date_time();
	if (g_num > 0) {
		upload_json(buf, g_num);
		g_num = 0;	
	}
}
