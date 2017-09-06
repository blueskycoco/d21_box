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
	gprs_config();
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
	uint32_t serial_no[4];
	uint8_t page_data[EEPROM_PAGE_SIZE];	
	struct rtc_calendar_time time;

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
	configure_eeprom();
	configure_bod();
	eeprom_emulator_read_page(0, page_data);
	history_num = (page_data[0]<<8) | page_data[1];
	printf("cur history num: %d\r\n",history_num);
	test_gprs();
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
		//get_rtc_time(&time);		
		sleepmgr_enter_sleep();
	}
}

