#ifndef _GPRS_H
#define _GPRS_H
uint8_t gprs_config(void);
uint8_t http_post(uint8_t *data, int len, char *rcv);
void test_gprs(void);
uint8_t upload_data(char *json, uint32_t *time);
#endif
