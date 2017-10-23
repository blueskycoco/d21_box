#include "submit.h"
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
		};
		unsigned char bytes[8];
	} record;
} streamer;

void apollo_byte_stream_reset(void)
{
	streamer.state = 0;
	streamer.length = 0;
	streamer.data_index = 0;
	streamer.len_index = 0;
	streamer.type = 0;
	streamer.record.cate = 0;
	streamer.record.data = 0;
	streamer.record.num = 0;
	streamer.record.time = 0;
}

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
				 	submit_recorder(streamer.record.cate, streamer.record.data, streamer.record.num, streamer.record.time);
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

