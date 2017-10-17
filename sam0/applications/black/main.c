#include <asf.h>
#include <string.h>
#include "conf_usb_host.h"
#include "conf_bootloader.h"
#include "main.h"
#include "misc.h"
#include "gprs.h"
#include "usb.h"
#include "rtc.h"
#include "flash.h"
#include "history.h"
#include "calendar.h"
#define MAX_JSON 26
#define MAX_TRY 3
extern bool libre_found;
#if CONSOLE_OUTPUT_ENABLED
#define APP_HEADER "samd21 black box\r\n"
#endif

int32_t cur_ts = -1;
int32_t bak_ts = -1;
extern uint8_t cur_libre_serial_no[32];
//COMPILER_WORD_ALIGNED
//volatile uint8_t buffer[FLASH_BUFFER_SIZE];
char device_serial_no[33] = {0};
static void black_system_init(void)
{
	struct nvm_config nvm_cfg;
	struct port_config pin;

	/* Initialize the system */
	system_init();

	/*Configures PORT for LED0*/
	port_get_config_defaults(&pin);
	pin.direction = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(LED0_PIN, &pin);

	/* Initialize the NVM */
	nvm_get_config_defaults(&nvm_cfg);
	nvm_cfg.manual_page_write = false;
	nvm_set_config(&nvm_cfg);

	/* Initialize the sleep manager */
	sleepmgr_init();

#if CONSOLE_OUTPUT_ENABLED
	/* Initialize the console */
	console_init();
	/* Wait stdio stable */
	delay_init();
	delay_ms(5);
	/* Print a header */
	printf(APP_HEADER);
#endif
	/* Enable the global interrupts */
	cpu_irq_enable();
	
	/* Start USB host stack */
	uhc_start();
	gprs_config();
	if (history_init())
		printf("load history done\r\n");
}

void ui_usb_connection_event(uhc_device_t *dev, bool b_present)
{
	UNUSED(dev);
	if (!b_present) {
		libre_found = false;		
		//cur_ts = -1;
		bak_ts = -1;
	}
}
void ui_usb_wakeup_event(void)
{
	printf("usb wakeup event\r\n");
}

int main(void)
{
	char *json = NULL;
	uint8_t *xt_data = NULL;
	uint32_t xt_len = 0;
	uint32_t serial_no[4];
	char out[256] = {0};
	struct calendar_date date;
	struct rtc_calendar_time rtc_time;
	uint8_t result = 0;
	int32_t ts;
	unsigned int i,j;

	serial_no[0] = *(uint32_t *)0x0080A00C;
	serial_no[1] = *(uint32_t *)0x0080A040;
	serial_no[2] = *(uint32_t *)0x0080A044;
	serial_no[3] = *(uint32_t *)0x0080A048;
	sprintf(device_serial_no, "%08x%08x%08x%08x",
			(unsigned)serial_no[0], (unsigned)serial_no[1],
			(unsigned)serial_no[2], (unsigned)serial_no[3]);
	black_system_init();
	printf("ID %x %x %x %x \r\n", (unsigned int)serial_no[0],
			(unsigned int)serial_no[1], (unsigned int)serial_no[2], (unsigned int)serial_no[3]);
	init_rtc();
	while (true) {
		if (libre_found) {
			if (uhc_is_suspend())
				uhc_resume();
			if (!uhc_is_suspend()) {				
				ui_init();
				if (cur_ts == -1 || bak_ts == -1) {
					cur_ts = get_dev_ts(cur_libre_serial_no,32);
					bak_ts = cur_ts;
					printf("%s inserted , ts %d\r\n",cur_libre_serial_no,(int)cur_ts);
				}
				if (1/*get_cap_data(&xt_data, &xt_len)*/) {
					j = 0;
					uint32_t time = 0;
					printf("get %d bytes from sugar\r\n", (int)xt_len);
					for (i = 0; i < xt_len;) {
						/* |**|****|*| */
						ts = xt_data[i+2] << 24 | 
							 xt_data[i+3] << 16 | 
							 xt_data[i+4] <<  8 | 
							 xt_data[i+5] <<  0;
						if (ts > cur_ts) {
							printf("add ts %d to list\r\n", (int)ts);
							int16_t gid = xt_data[i] << 8 | xt_data[i+1];
							uint32_t bloodSugar = xt_data[i+6]*142+22;
							json = build_json(json, 0, bloodSugar, ts, gid, 
											device_serial_no);
							j++;
							if (j >= MAX_JSON) {
								int n = 0;
								while (n < MAX_TRY) {
									memset(out,0,256);
									result = http_post((uint8_t *)json,strlen(json)
																,out);
									if (result) {
										do_it((uint8_t *)out, &time);
										printf("send server %s, \r\nrcv %s, time %d\r\n",
												json, out, (int)time);
										bak_ts = ts;
										break;
									}
									n++;
								}
								free(json);
								json = NULL;
								j=0;
								if (n == MAX_TRY)
									break;
							}
						}
						i=i+7;
					}
					if (j > 0) {
						int n = 0;
						while (n < MAX_TRY) {
							memset(out,0,256);
							result = http_post((uint8_t *)json,strlen(json),out);
							if (result) {
								do_it((uint8_t *)out, &time);
								printf("send server %s, rcv %s, time %d\r\n",
											json, out, (int)time);
								bak_ts = ts;
								break;
							}
							n++;
						}
						free(json);
						json = NULL;
						j=0;
					}
					/*set cur time*/
					if (time != 0) {
						calendar_timestamp_to_date(time, &date);
						rtc_time.minute = date.minute;
						rtc_time.second = date.second;
						rtc_time.year = date.year;
						rtc_time.month = date.month;
						rtc_time.day = date.date+1;
						rtc_time.hour = date.hour;
						set_rtc_time(rtc_time);
					}
					if (bak_ts > cur_ts) {
						cur_ts = bak_ts;
						save_dev_ts(cur_ts);
						printf("save ts %d\r\n", (int)cur_ts);
					}
				} else
					printf("there is no data from sugar\r\n");
				printf("suspend usb\r\n");
				uhc_suspend(false);
			}
		} else
			printf("no sugar insert\r\n");
		sleepmgr_enter_sleep();
	}
}

