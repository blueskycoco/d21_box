/**
 * \file
 *
 * \brief User Interface
 *
 * Copyright (C) 2014-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include <asf.h>
#include "box_usb.h"
#include "calendar.h"

extern bool libre_found;
unsigned char response[64] = {0};

unsigned char recv_final_flag = 0;

unsigned char recv_0x0c_cnt = 0;
unsigned char first_send_28 = 1;
union apollo_report
{
    unsigned int report_word[16];
    unsigned char report_byte[63];
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

void submit_serial(unsigned char * serial)
{
	printf("Serial Number: %s\r\n", serial);
}

void submit_recorder(unsigned char cate, unsigned char data, unsigned short num, unsigned int time)
{
	struct calendar_date date_out;
	calendar_timestamp_to_date(time, &date_out);
	printf("Num\t%5d\tTime\t%4d-%02d-%02d %02d:%02d:%02d\tCate:\t%1d\tData\t%3d\r\n", num, date_out.year,
			date_out.month, date_out.date, date_out.hour, date_out.minute, date_out.second, cate, data);
}

void submit_date_time(unsigned char second, unsigned char minute, unsigned char hour, unsigned char day, unsigned char month, unsigned int year)
{
	printf("Date-Time is: %4d-%02d-%02d %2d:%02d:%02d\r\n", year, month, day, hour, minute, second);
}

struct stream_handler
{
	int state;
	int length;
	int data_index;
	int len_index;
	unsigned char type;
	union
	{
		struct
		{
			unsigned char cate;
			unsigned char data;
			unsigned short num;
			unsigned int time;
			uint32_t record_len;
		};
		unsigned char bytes[8];
	} record;
} streamer;

void apollo_byte_stream_push(unsigned char byte)
{
#define HANDLER_IDLE	(0)
#define HANDLER_LEN	(1)
#define HANDLER_TYPE	(2)
#define HANDLER_DATA	(3)
	switch(streamer.state)
	{
	case HANDLER_IDLE:
		if(byte > 0x80)
		{
			streamer.state = HANDLER_LEN;
			streamer.length = byte & 0x7f;
			streamer.len_index = 1;
		}
		break;
	case HANDLER_LEN:
		if(byte > 0x80)
		{
			streamer.state = HANDLER_LEN;
			streamer.length += (byte & 0x7f) << (7 * streamer.len_index);
			break;
		}
		streamer.state = HANDLER_TYPE;
	case HANDLER_TYPE:
		streamer.type = byte;
		streamer.state = HANDLER_DATA;
		streamer.data_index = 0;
		streamer.record.cate = 0xFF;
		break;
	case HANDLER_DATA:
		if(streamer.type == 0x31)
		{
			switch(streamer.data_index)
			{
			case 1:  streamer.record.bytes[2] = byte; break;
			case 2:  streamer.record.bytes[3] = byte; break;
			case 3:  streamer.record.cate = byte == 0x0C ? 0 : byte == 0x02 ? 1 : 0xFF; break;
			case 4:  if(byte != 0x80) streamer.record.cate = 0xFF; break;
			case 5:  streamer.record.bytes[4] = byte; break;
			case 6:  streamer.record.bytes[5] = byte; break;
			case 7:  streamer.record.bytes[6] = byte; break;
			case 8:  streamer.record.bytes[7] = byte; break;
			case 9:  streamer.record.time += ((unsigned int)byte); break;
			case 10: streamer.record.time += ((unsigned int)byte) << 8; break;
			case 11: streamer.record.time += ((unsigned int)byte) << 16; break;
			case 12: streamer.record.time += ((unsigned int)byte) << 24; break;
			case 13: streamer.record.bytes[1] = byte;
				 if(streamer.record.cate != 0xFF && streamer.record.data != 0)
				 {
				 	submit_recorder(streamer.record.cate, streamer.record.data, streamer.record.num, streamer.record.time);
					streamer.record.record_len++;
				 }
			default: break;
			}
		}
		streamer.data_index ++;
		streamer.length --;
		if(streamer.length == 0)
		{
			streamer.state = HANDLER_IDLE;
			streamer.data_index = 0;
			streamer.len_index = 0;			
		}
		break;
	default:
		break;
	}
}

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


static void apollo_sync(unsigned char class)
{
    glucose_usb_data.report_out.report = 0x0D;
    glucose_usb_data.report_out.length = 4;
    glucose_usb_data.report_out.remote_order = glucose_usb_data.upstream_order;
    glucose_usb_data.report_out.local_order = glucose_usb_data.downstream_order;
    glucose_usb_data.report_out.report_byte[4] = 0;
    glucose_usb_data.report_out.report_byte[5] = class;
    usb_send_report(glucose_usb_data.report_out.report_byte);
    for(int i=0;i<14;i++)
        glucose_usb_data.report_out.report_word[i] = 0;

    glucose_usb_data.length_out = 0;
}

static void apollo_send_raw_packet(unsigned char * data, int length)
{
    for(int i=0; i < length; i++)
        glucose_usb_data.report_out.report_byte[i] = data[i];

    usb_send_report(glucose_usb_data.report_out.report_byte);

    for(int i=0;i<14;i++)
        glucose_usb_data.report_out.report_word[i] = 0;
}

static void apollo_read_raw_packet()
{
    usb_read_report(glucose_usb_data.report_in.report_byte);
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

static void apollo_flush_report_out()
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
static void apollo_fetch_report_in()
{
    int pending = 1;
    while(pending)
    {
    	if (!libre_found)
			break;
        usb_read_report(glucose_usb_data.report_in.report_byte);
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
            if(glucose_usb_data.upstream_order & 0x3)
                apollo_sync(0);
            break;
        case 0x0C:
            apollo_sync(0);
            break;
        }
    }
}

static void apollo_init()
{
    for(int i=0;i<14;i++)
        glucose_usb_data.report_out.report_word[i] = 0;
    glucose_usb_data.length_out = 0;
    glucose_usb_data.upstream_order = 0;
    glucose_usb_data.downstream_order = 0;

    unsigned char cmd[2];
    cmd[0] = 0x04; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();

    apollo_sync(2);
	delay_ms(200);
    //serial number
    cmd[0] = 0x05; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();
	submit_serial(glucose_usb_data.report_in.report_byte + 2);
    //software version
    cmd[0] = 0x15; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();

    cmd[0] = 0x01; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();

//	unsigned char dbrnum[] = {0x21, 0x08, 0x24, 0x64, 0x62, 0x72, 0x6E, 0x75, 0x6D, 0x3F};
//	apollo_send_raw_packet(dbrnum, 10);
//	apollo_read_raw_packet();
}

static void apollo_req_recorder(void)
{
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
		apollo_fetch_report_in();
		for(int i = 0; i < glucose_usb_data.report_in.length - 6; i++)
			apollo_byte_stream_push(glucose_usb_data.report_in.payload_byte[i]);

	}while(glucose_usb_data.report_in.length == 0x3e);
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
}

void ui_init()
{
	static bool init = false;
	if (init || !libre_found)
		return;
	init = true;
    apollo_init();

    apollo_req_recorder();
	apollo_req_date_time();
	
	printf("record_len = %d\n", streamer.record.record_len);
}
