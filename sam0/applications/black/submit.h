#ifndef __SUBMIT_H__
#define __SUBMIT_H__

void submit_serial(char * serial);
void submit_recorder(unsigned char cate, unsigned short data, unsigned short num, unsigned int time);
void submit_date_time(unsigned char second, unsigned char minute, unsigned char hour, unsigned char day, unsigned char month, unsigned int year);

#endif

