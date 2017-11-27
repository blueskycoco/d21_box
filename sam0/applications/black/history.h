#ifndef _HISTORY_H
#define _HISTORY_H
uint8_t history_init(void);
uint32_t get_dev_ts(uint8_t *serial, uint8_t len);
uint16_t get_dev_gid();
void save_dev_ts(uint32_t ts,uint16_t gid);
void at25dfx_off(void);
void at25dfx_on(void);
#endif
