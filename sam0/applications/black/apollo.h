#ifndef __APOLLO_H__
#define __APOLLO_H__

void apollo_req_recorder_all(void);
void apollo_req_recorder_single(void);
void apollo_req_recorder_history(void);
void apollo_req_date_time(void);
void apollo_set_date_time(unsigned char second, unsigned char minute, unsigned char hour, unsigned char day, unsigned char month, unsigned int year);

#endif

