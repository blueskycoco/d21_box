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
static void apollo_append_report_out(unsigned char * data, int length)
{
    for(int i=0; i < length; i++)
    {
        if(glucose_usb_data.length_out == 0)//double first byte fit up to 127 bytes length
        {
            glucose_usb_data.report_out.payload_byte[glucose_usb_data.length_out] = data[i];
            glucose_usb_data.length_out ++;
        }
        glucose_usb_data.report_out.payload_byte[glucose_usb_data.length_out] = data[i];
        glucose_usb_data.length_out ++;
    }
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

    //serial number
    cmd[0] = 0x05; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();
    //software version
    cmd[0] = 0x15; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();

    cmd[0] = 0x01; cmd[1] = 0x00;
    apollo_send_raw_packet(cmd, 2);
    apollo_read_raw_packet();

    unsigned char dbrnum[10] = {0x21, 0x08, 0x24, 0x64, 0x62, 0x72, 0x6E, 0x75, 0x6D, 0x3F};
    apollo_send_raw_packet(dbrnum, 10);
    apollo_read_raw_packet();
}
static void apollo_get_recorder()
{
    //get first packet
    do
    {
        apollo_fetch_report_in();
    }while(glucose_usb_data.report_in.length == 0x3e);
/*
    int bytes_left = 0, i = 0;
    while(glucose_usb_data.report_in.payload_byte[i] >= 128)
    {
        bytes_left = (glucose_usb_data.report_in.payload_byte[i] & 0x7F) << (i*7);
        i++;
    }
    bytes_left -= (glucose_usb_data.report_in.length - 6);

    while(bytes_left > 0)
    {
        apollo_fetch_report_in();
        bytes_left -= (glucose_usb_data.report_in.length - 6);
    }
*/
}

static void apollo_req_recorder_1byte(unsigned char type)
{
    apollo_append_report_out(&type, 1);
    apollo_flush_report_out();

    type = 0x7D;
    apollo_append_report_out(&type, 1);
    apollo_flush_report_out();

    apollo_get_recorder();
}


static void apollo_req_recorder_2byte(unsigned char typel, unsigned char typeh)
{
    apollo_append_report_out(&typel, 1);
    apollo_append_report_out(&typeh, 1);
    apollo_flush_report_out();

    typel = 0x7D;
    apollo_append_report_out(&typel, 1);
    apollo_flush_report_out();

    apollo_get_recorder();
}
void ui_init()
{
	static bool init = false;
	if (init)
		return;
	init = true;
    apollo_init();

    apollo_req_recorder_2byte(0x31, 0x00);//读取血糖历史数据
    apollo_req_recorder_2byte(0x31, 0x06);//读取血糖单次测量数据
    apollo_req_recorder_1byte(0x41);//读取时间
}
