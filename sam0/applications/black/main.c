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
#define MAX_JSON 20
extern bool libre_found;
#if CONSOLE_OUTPUT_ENABLED
#define APP_HEADER "samd21 black box\r\n"
#endif

int32_t cur_ts = -1;
int32_t bak_ts = -1;
uint32_t server_time = 0;
extern uint8_t cur_libre_serial_no[32];
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
		memset(cur_libre_serial_no,0,32);
	}
}
void ui_usb_wakeup_event(void)
{
	printf("usb wakeup event\r\n");
}
void upload_json(uint8_t *xt_data, uint32_t xt_len)
{
	char *json = NULL;
	int32_t ts;
	unsigned int i,upload_num=0;
	
	if (!xt_data || xt_len == 0)
		return;
	
	for (i = 0; i < xt_len;) {
		/* |**|****|*| */
		ts = xt_data[i+2] << 24 | 
			 xt_data[i+3] << 16 | 
			 xt_data[i+4] <<  8 | 
			 xt_data[i+5] <<  0;
		if (ts > cur_ts) {
			/*check ts > last ts*/
			int16_t gid = xt_data[i] << 8 | xt_data[i+1];
			uint32_t bloodSugar = xt_data[i+6]*142+22;
			//printf("add ts %d,gid %d, blood %d to list\r\n", (int)ts,
			//	(int)gid, (int)bloodSugar);
			json = build_json(json, 0, bloodSugar, ts, gid, 
							device_serial_no);
			upload_num++;
			//printf("num %d\r\n", upload_num);
			if (upload_num >= MAX_JSON) {
				/*json item > max_json*/
				/*if (upload_data(json,&server_time))	{
					if (ts > bak_ts)
						bak_ts = ts;
				}*/
				printf("\r\nbegin upload\r\n");
				printf("%s", json);
				free(json);
				json = NULL;
				upload_num=0;
			}
		}
		i=i+7;
	}
	printf("\r\nupload last data\r\n");
	if (upload_num > 0) {
		/*upload num < MAX_JSON*/
		/*if (upload_data(json,&server_time)) {
			if (ts > bak_ts)
				bak_ts = ts;
		}*/
		printf("%s\r\n",json);
		free(json);
		json = NULL;
		upload_num=0;
	}
}
static void update_time(uint32_t time)
{
	struct calendar_date date;
	struct rtc_calendar_time rtc_time;
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
	/*set sugar time*/

	if (bak_ts > cur_ts) {
		cur_ts = bak_ts;
		save_dev_ts(cur_ts);
		printf("save ts %d\r\n", (int)cur_ts);
	}
}
int main(void)
{
	uint32_t serial_no[4];
	serial_no[0] = *(uint32_t *)0x0080A00C;
	serial_no[1] = *(uint32_t *)0x0080A040;
	serial_no[2] = *(uint32_t *)0x0080A044;
	serial_no[3] = *(uint32_t *)0x0080A048;
	sprintf(device_serial_no, "%08x%08x%08x%08x",
			(unsigned)serial_no[0], (unsigned)serial_no[1],
			(unsigned)serial_no[2], (unsigned)serial_no[3]);
	black_system_init();
	printf("ID %x %x %x %x \r\n", (unsigned int)serial_no[0],
			(unsigned int)serial_no[1], (unsigned int)serial_no[2], 
			(unsigned int)serial_no[3]);
	init_rtc();
	while (true) {
		if (libre_found) {
			if (uhc_is_suspend())
				uhc_resume();
			delay_ms(100);
			if (!uhc_is_suspend()) {
				if (strlen(cur_libre_serial_no) == 0)
					if (!apollo_init())
						memset(cur_libre_serial_no, 0, 32);
					
				if (strlen(cur_libre_serial_no) != 0) {
					if (cur_ts == -1 || bak_ts == -1) {
						cur_ts = get_dev_ts(cur_libre_serial_no,32);
						bak_ts = cur_ts;
						printf("%s inserted , ts %d\r\n",
								cur_libre_serial_no,(int)cur_ts);
					}
					printf("cur ts %d %d\r\n", cur_ts,bak_ts);
					ui_init();
					if (cur_ts < bak_ts)
						update_time(server_time);
				}
				//printf("suspend usb\r\n");
				uhc_suspend(false);
			}
		} else
			printf("no sugar insert\r\n");
		sleepmgr_enter_sleep();
	}
}

