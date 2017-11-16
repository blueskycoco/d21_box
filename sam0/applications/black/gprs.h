#ifndef _GPRS_H
#define _GPRS_H
uint8_t gprs_config(void);
static uint32_t http_post(uint8_t *data, int len);
void test_gprs(void);
void gprs_power(int on);
uint8_t upload_data(char *json, uint32_t *time);
void gprs_init(void);
void gprs_test(void);
#endif
