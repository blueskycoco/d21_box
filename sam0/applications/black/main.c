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

extern bool libre_found;
#if CONSOLE_OUTPUT_ENABLED
#define APP_HEADER "samd21 black box\r\n"
#endif
static int history_num = 0;
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
	//ui_init();
	/* Start USB host stack */
	uhc_start();
	//gprs_config();
	if (history_init())
		printf("load history done\r\n");
}

void ui_usb_connection_event(uhc_device_t *dev, bool b_present)
{
	UNUSED(dev);
	if (!b_present) {
		libre_found = false;		
	}
}
//bool usb_sleeping = false;
void ui_usb_wakeup_event(void)
{
	printf("usb wakeup event\r\n");
	//usb_sleeping = false;
}

int main(void)
{
	char *json = NULL;
	uint32_t serial_no[4];
	uint8_t page_data[4096]={0};	
	struct rtc_calendar_time time;
	char type = 0;
	int bloodSugar[2] = {0};
	//int actionTime = 0x1234;
	int gid = 0,ggid=0;
	int i;
	
	serial_no[0] = *(uint32_t *)0x0080A00C;
	serial_no[1] = *(uint32_t *)0x0080A040;
	serial_no[2] = *(uint32_t *)0x0080A044;
	serial_no[3] = *(uint32_t *)0x0080A048;
	sprintf(device_serial_no, "%08x%08x%08x%08x",
			(unsigned)serial_no[0], (unsigned)serial_no[1],
			(unsigned)serial_no[2], (unsigned)serial_no[3]);
	black_system_init();
	printf("ID %x %x %x %x \r\n", serial_no[0],
			serial_no[1], serial_no[2], serial_no[3]);
	init_rtc();
	struct rtc_calendar_time cur_time;
	get_rtc_time(&cur_time);
	printf("set time %d\r\n",set_rtc_time(cur_time));
//	configure_eeprom();
	//configure_bod();
	//eeprom_emulator_read_page(0, page_data);
	printf("cur history num: %d\r\n",history_num);
	//test_gprs();
	while (true) {
		if (libre_found) {
			if (uhc_is_suspend())
				uhc_resume();
			if (!uhc_is_suspend())
			{
				get_cap_data();
				printf("suspend usb\r\n");
				uhc_suspend(false);
			}
		}
		get_rtc_time(&time);	
		sprintf(page_data,"%d.%d",bloodSugar[0],bloodSugar[1]);
		json = build_json(json, type, page_data, date2ts(time), ggid, device_serial_no);
		if (json != NULL)
		printf("%s\r\n",json);
		type = 1 - type;
		bloodSugar[0] +=1;
		bloodSugar[1] +=2;
		//actionTime +=1;
		gid +=1;
		ggid +=1;
		if (gid >= 30)
		{	
			//if (ggid > 1)
			//	ggid -=1;						
			sprintf(page_data, "hello test %d", ggid);
			free(json);
			json = NULL;
			gid = 0;
		}
		else			
			sprintf(page_data, "hello test %d", gid);
		//printf("ts %d\r\n",get_dev_ts(page_data,strlen(page_data)));
		
		sleepmgr_enter_sleep();
	}
}

