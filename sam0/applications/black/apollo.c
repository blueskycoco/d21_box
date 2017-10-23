#include <stdio.h>
#include <asf.h>
#include "stream.h"
#include "submit.h"
extern bool libre_found;
union apollo_report
{
	unsigned int report_word[16];
	char report_byte[64];
	struct
	{
		unsigned char report;
		unsigned char length;
		unsigned char remote_order;
		unsigned char local_order;
		unsigned int crc_unique;
		union
		{
			unsigned int payload_word[14];
			unsigned char payload_byte[56];
		};
	};
};

struct apollo_usb_data
{
	union apollo_report report_out;
	int length_out;
	union apollo_report report_in;
	int length_in;
	unsigned char upstream_order;
	unsigned char downstream_order;
} glucose_usb_data;

static unsigned int crc_unique(unsigned int * data, unsigned int word_count)
{
	const unsigned int poly[16] =
	{
		0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
		0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd
	};
	unsigned int remainder= 0xffffffffL;

	for(unsigned int i=0;i<word_count;i++)
	{
		remainder ^= data[i];
		for(int j=0;j<8;j++)
			remainder = (remainder << 4) ^ poly[remainder >> 28];
	}

	return remainder;
}


static bool apollo_sync(unsigned char class)
{
	glucose_usb_data.report_out.report = 0x0D;
	glucose_usb_data.report_out.length = 4;
	glucose_usb_data.report_out.remote_order = glucose_usb_data.upstream_order;
	glucose_usb_data.report_out.local_order = glucose_usb_data.downstream_order;
	glucose_usb_data.report_out.report_byte[4] = 0;
	glucose_usb_data.report_out.report_byte[5] = class;
	bool ret = usb_send_report(glucose_usb_data.report_out.report_byte);
	for(int i=0;i<14;i++)
		glucose_usb_data.report_out.report_word[i] = 0;
	glucose_usb_data.length_out = 0;

	return ret;
}

static bool apollo_send_raw_packet(unsigned char * data, int length)
{
	for(int i=0; i < length; i++)
		glucose_usb_data.report_out.report_byte[i] = data[i];

	bool ret = usb_send_report(glucose_usb_data.report_out.report_byte);

	for(int i=0;i<14;i++)
		glucose_usb_data.report_out.report_word[i] = 0;

	return ret;
}

static bool apollo_read_raw_packet(void)
{
	return usb_read_report(glucose_usb_data.report_in.report_byte);
}

void apollo_append_report_out(unsigned char data)
{
	if(glucose_usb_data.length_out == 0)//double first byte fit up to 127 bytes length
	{
		glucose_usb_data.report_out.payload_byte[glucose_usb_data.length_out] = data;
		glucose_usb_data.length_out ++;
	}
	glucose_usb_data.report_out.payload_byte[glucose_usb_data.length_out] = data;
	glucose_usb_data.length_out ++;
}

static void apollo_flush_report_out(void)
{
	if(glucose_usb_data.length_out == 0)
	{
		return;
	}
	else if(glucose_usb_data.length_out == 2) // 1 byte only
	{
		glucose_usb_data.length_out = 1;
		glucose_usb_data.report_out.payload_byte[1] = 0;
	}
	else
	{
		glucose_usb_data.report_out.payload_byte[0] = (glucose_usb_data.length_out - 2) | 0x80;
	}
	glucose_usb_data.report_out.report = 0x0A;
	glucose_usb_data.report_out.length = glucose_usb_data.length_out + 6;
	glucose_usb_data.report_out.remote_order = glucose_usb_data.upstream_order;
	glucose_usb_data.report_out.local_order = glucose_usb_data.downstream_order;
	glucose_usb_data.report_out.crc_unique = crc_unique(glucose_usb_data.report_out.payload_word, (glucose_usb_data.length_out + 3) >> 2);

	usb_send_report(glucose_usb_data.report_out.report_byte);

	for(int i=0;i<14;i++)
		glucose_usb_data.report_out.report_word[i] = 0;
	glucose_usb_data.downstream_order ++;
	glucose_usb_data.length_out = 0;
}

static bool apollo_fetch_report_in(void)
{
	int pending = 1;
	bool ret = true;
	while(pending && ret && libre_found)
	{
		ret = usb_read_report(glucose_usb_data.report_in.report_byte);
		switch(glucose_usb_data.report_in.report)
		{
		case 0x0B: 
			if(glucose_usb_data.report_in.local_order != glucose_usb_data.upstream_order)
			{
				apollo_sync(0);
				break;
			}
			pending = 0;
			glucose_usb_data.upstream_order ++;
			if((glucose_usb_data.upstream_order & 0x3) == 0)
				apollo_sync(0);
			break;
		case 0x0C:
			apollo_sync(0);
			break;
		}
	}

	return ret;
}

bool apollo_init(void)
{
	bool ret = false;
	for(int i=0;i<14;i++)
		glucose_usb_data.report_out.report_word[i] = 0;
	glucose_usb_data.length_out = 0;
	glucose_usb_data.upstream_order = 0;
	glucose_usb_data.downstream_order = 0;

	unsigned char cmd[2];
	cmd[0] = 0x04; cmd[1] = 0x00;
	ret = apollo_send_raw_packet(cmd, 2);
	if (ret)
		ret = apollo_read_raw_packet();
	
	if (ret)
		ret = apollo_sync(2);
	
	delay_ms(200);
	//serial number
	cmd[0] = 0x05; cmd[1] = 0x00;
	if (ret)
	ret = apollo_send_raw_packet(cmd, 2);
	if (ret)
	ret = apollo_read_raw_packet();
	submit_serial(glucose_usb_data.report_in.report_byte + 2);
	//software version
	cmd[0] = 0x15; cmd[1] = 0x00;
	if (ret)
	ret = apollo_send_raw_packet(cmd, 2);
	if (ret)
	ret = apollo_read_raw_packet();
	
	cmd[0] = 0x01; cmd[1] = 0x00;
	if (ret)
	ret = apollo_send_raw_packet(cmd, 2);
	if (ret)
	ret = apollo_read_raw_packet();
	return ret;
//	unsigned char dbrnum[] = {0x21, 0x08, 0x24, 0x64, 0x62, 0x72, 0x6E, 0x75, 0x6D, 0x3F};
//	apollo_send_raw_packet(dbrnum, 10);
//	apollo_read_raw_packet();
}

void apollo_req_recorder_all(void)
{
	bool ret = true;
	apollo_append_report_out(0x31);
	apollo_append_report_out(0x00);
	apollo_flush_report_out();
	apollo_append_report_out(0x31);
	apollo_append_report_out(0x06);
	apollo_flush_report_out();
	apollo_append_report_out(0x7D);
	apollo_flush_report_out();
	do
	{
		ret = apollo_fetch_report_in();
		for(int i = 0; i < glucose_usb_data.report_in.length - 6; i++)
			apollo_byte_stream_push(glucose_usb_data.report_in.payload_byte[i]);

	}while(glucose_usb_data.report_in.length == 0x3e && ret && libre_found);
	apollo_sync(0);
}

static void apollo_req_recorder_single(void)
{
	bool ret = true;
	apollo_append_report_out(0x31);
	apollo_append_report_out(0x00);
	apollo_flush_report_out();
	apollo_append_report_out(0x7D);
	apollo_flush_report_out();
	do
	{
		ret = apollo_fetch_report_in();
		for(int i = 0; i < glucose_usb_data.report_in.length - 6; i++)
			apollo_byte_stream_push(glucose_usb_data.report_in.payload_byte[i]);

	}while(glucose_usb_data.report_in.length == 0x3e && ret && libre_found);
	apollo_sync(0);
}

static void apollo_req_recorder_history(void)
{
	bool ret = true;
	apollo_append_report_out(0x31);
	apollo_append_report_out(0x06);
	apollo_flush_report_out();
	apollo_append_report_out(0x7D);
	apollo_flush_report_out();
	do
	{
		ret = apollo_fetch_report_in();
		for(int i = 0; i < glucose_usb_data.report_in.length - 6; i++)
			apollo_byte_stream_push(glucose_usb_data.report_in.payload_byte[i]);

	}while(glucose_usb_data.report_in.length == 0x3e && ret && libre_found);
	apollo_sync(0);
}

void apollo_req_date_time(void)
{
	apollo_append_report_out(0x41);
	apollo_flush_report_out();
	apollo_append_report_out(0x7D);
	apollo_flush_report_out();
	apollo_fetch_report_in();
	submit_date_time(
			glucose_usb_data.report_in.payload_byte[2], 
			glucose_usb_data.report_in.payload_byte[3], 
			glucose_usb_data.report_in.payload_byte[4], 
			glucose_usb_data.report_in.payload_byte[5], 
			glucose_usb_data.report_in.payload_byte[6], 
			glucose_usb_data.report_in.payload_byte[7] | glucose_usb_data.report_in.payload_byte[8] << 8
			);
	apollo_sync(0);
}

void apollo_set_date_time(unsigned char second, unsigned char minute, unsigned char hour, unsigned char day, unsigned char month, unsigned int year)
{
	apollo_append_report_out(0x42);
	apollo_append_report_out(second);
	apollo_append_report_out(minute);
	apollo_append_report_out(hour);
	apollo_append_report_out(day);
	apollo_append_report_out(month);
	apollo_append_report_out(year & 0xFF);
	apollo_append_report_out(year >> 8);
	apollo_append_report_out(0x01);
	apollo_flush_report_out();
	apollo_append_report_out(0x7D);
	apollo_flush_report_out();
	apollo_fetch_report_in();
	apollo_sync(0);
}

