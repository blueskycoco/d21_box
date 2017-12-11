#include <asf.h>
#include <string.h>
#include "conf_usb_host.h"
#include "conf_bootloader.h"
#include "main.h"
#include "misc.h"
#include "gprs.h"
#include "usb.h"
#include "rtc.h"
#include "history.h"
#include "calendar.h"
#include "apollo.h"
#define MAX_JSON 1//20
extern bool libre_found;
bool long_press = false;
extern char *imsi_str;
#if CONSOLE_OUTPUT_ENABLED
#define APP_HEADER "samd21 black box\r\n"
#endif
int32_t g_hour = -1;

int32_t cur_ts = -1;
int32_t bak_ts = -1;
int32_t max_ts = 0;
uint16_t cur_gid = -1;
uint16_t bak_gid = -1;
uint16_t max_gid = 0;
uint32_t server_time = 0;
uint32_t serial_no[4];
extern uint8_t *buf;
extern uint8_t *send;
extern uint8_t cur_libre_serial_no[32];
uint8_t *xt_data_now=NULL;
uint32_t xt_len_now=0;
extern void ts2date(uint32_t time, struct rtc_calendar_time *date_out);
char device_serial_no[33] = {0};
static void set_unused_io(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);
	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	port_pin_set_config(PIN_PA02, &pin_conf);
	port_pin_set_config(PIN_PA03, &pin_conf);
	port_pin_set_config(PIN_PA06, &pin_conf);
	port_pin_set_config(PIN_PA28, &pin_conf);
}
static void black_system_init(void)
{
	struct nvm_config nvm_cfg;

	/* Initialize the system */
	system_init();

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
	set_unused_io();	
	gprs_init();
	port_pin_set_output_level(PIN_PA07, true);
	/* Start USB host stack */
	//uhc_start();
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
		bak_gid = -1;
		memset(cur_libre_serial_no,0,32);
		printf("usb disconnect\r\n");
	} else
		printf("usb connect\r\n");
}
void ui_usb_wakeup_event(void)
{
	printf("usb wakeup event\r\n");
}


/**
 * \brief Initializes and enables interrupt pin change
 */
#define LINE 9
static void usb_power(int on)
{
	if (on) {
		port_pin_set_output_level(PIN_PA02, false);
		port_pin_set_output_level(PIN_PA03, true);
		port_pin_set_output_level(PIN_PA27, true);
	} else {
		port_pin_set_output_level(PIN_PA27, false);
		//port_pin_set_output_level(PIN_PA02, false);
		port_pin_set_output_level(PIN_PA03, false);
	}

}
static void power_off()
{
	port_pin_set_output_level(PIN_PA07, false);
}
static void button_callback(void)
{
	printf("button pressed\r\n");
	/*long press for power off*/
	/*short press for cap*/
	uint32_t i = 0;

	while(1) {
		if (!port_pin_get_input_level(PIN_PA09))
			i++;
		else
			break;
		delay_ms(1);
		if (i>1000)
			break;
	}

	if (i > 1000)
		long_press = true;
	else
		long_press = false;
	printf("button pressed %d\r\n",long_press);	
	if (long_press) {	
		usb_power(0);
		gprs_power(0);
		power_off();
	}
}
static void enable_button_interrupt(void)
{
	/* Initialize EIC for button wakeup */
	struct extint_chan_conf eint_chan_conf;
	extint_chan_get_config_defaults(&eint_chan_conf);

	eint_chan_conf.gpio_pin            = PIN_PA09A_EIC_EXTINT9;
	eint_chan_conf.gpio_pin_mux        = MUX_PA09A_EIC_EXTINT9;
	eint_chan_conf.detection_criteria  = EXTINT_DETECT_FALLING;
	eint_chan_conf.filter_input_signal = true;
	extint_chan_set_config(LINE, &eint_chan_conf);
	extint_register_callback(button_callback,
			LINE,
			EXTINT_CALLBACK_TYPE_DETECT);
	extint_chan_clear_detected(LINE);
	extint_chan_enable_callback(LINE,
			EXTINT_CALLBACK_TYPE_DETECT);
}

//char json[2086] = {0};
char *json = NULL;
void upload_json(uint8_t *xt_data, uint32_t xt_len)
{
	
	int32_t ts,gid;
	unsigned int i,upload_num=0;	
	uint32_t total_len = 0;
	
	if (!xt_data || xt_len == 0)
		return;
	memset(json,0,1286);	
	xt_len_now=0;
	for (i = 0; i < xt_len;) {
		/* |**|****|*| */
		ts = xt_data[i+2] << 24 | 
			 xt_data[i+3] << 16 | 
			 xt_data[i+4] <<  8 | 
			 xt_data[i+5] <<  0;
		gid = (xt_data[i] << 8) | xt_data[i+1];
		if (ts > cur_ts || gid > cur_gid) {
			/*check ts > last ts*/
			if (ts > max_ts)
				max_ts = ts;
			if (gid > max_gid)
				max_gid = gid;
			memcpy(xt_data_now+xt_len_now, xt_data+i,11);
			xt_len_now += 11;
		}
		i=i+11;
	}
	/*store data & toHex 
	   id_len0|id_len1|serial_no|imsi_len0|imsi_len1|imsi|mcu_ID ..64.|len0|len1|len2|len3|data0|...|datan
	   */
	if (xt_len_now > 0) {
		gprs_power(1);
		gprs_config();
		uint32_t offs = 0;
		uint8_t device_id_len = strlen(cur_libre_serial_no);
		toHex(&device_id_len, 1, json);
		offs += 2;
		memcpy(json+offs, cur_libre_serial_no, device_id_len);
		offs += device_id_len;
		uint8_t imsi_len = strlen(imsi_str);
		printf("imsi_len %d\r\n",imsi_len);
		toHex(&imsi_len, 1, json+offs);
		offs += 2;
		memcpy(json+offs, imsi_str, imsi_len);
		offs += imsi_len;
		printf("json %s\r\n",json);
		uint8_t *serial_no_tmp = (uint8_t *)serial_no;
		uint8_t tmp1 = 0,tmp2 = 0;
		for (i=0;i<16;i=i+4)
		{
			tmp1 = serial_no_tmp[i];
			serial_no_tmp[i] = serial_no_tmp[i+3];
			serial_no_tmp[i+3] = tmp1;
			tmp2 = serial_no_tmp[i+1];
			serial_no_tmp[i+1] = serial_no_tmp[i+2];
			serial_no_tmp[i+2] = tmp2;
		}
		toHex((uint8_t *)serial_no_tmp, 16, json+offs);
		offs += 32;		
		uint32_t tmp = ((xt_len_now << 8)&0xff00) | ((xt_len_now>>8) & 0x00ff);
		toHex((uint8_t *)&tmp, 2,json+offs);
		offs += 4;
		toHex(xt_data_now, xt_len_now, json+offs);
		if (upload_data(json,&server_time))	{
			if (max_ts > bak_ts)
				bak_ts = max_ts;
			if (max_gid > bak_gid)
				bak_gid = max_gid;
		}
	}
	else
		printf("nothing to upload\r\n");
}
static void update_time(uint32_t time)
{
	struct rtc_calendar_time rtc_time;
	/*set cur time*/
	if (time != 0) {
		printf("begin to save time 1\r\n");
		ts2date(time, &rtc_time);
		printf("begin to save time 2 %d %d %d %d %d %d\r\n",
			rtc_time.second,rtc_time.minute,rtc_time.hour,
			rtc_time.day,rtc_time.month,rtc_time.year);
		//set_rtc_time(rtc_time);
		printf("begin to save time 3\r\n");
		apollo_set_date_time(rtc_time.second,rtc_time.minute,rtc_time.hour,
			rtc_time.day,rtc_time.month,rtc_time.year);
		printf("end save time\r\n");
	}

	if (bak_ts > cur_ts || bak_gid > cur_gid) {
		if (bak_ts > cur_ts)
			cur_ts = bak_ts;
		if (bak_gid > cur_gid)
			cur_gid = bak_gid;
		printf("save ts 1 %d %d\r\n", (int)cur_ts,(int)cur_gid);
		save_dev_ts(cur_ts,cur_gid);
		struct rtc_calendar_time rtc_time_now;
		get_rtc_time(&rtc_time_now);
		set_rtc_time();
		printf("save ts 2 %d %d\r\n", (int)cur_ts,(int)cur_gid);
	}
}
int main(void)
{	
	struct rtc_calendar_time rtc_time_now;
	serial_no[0] = *(uint32_t *)0x0080A00C;
	serial_no[1] = *(uint32_t *)0x0080A040;
	serial_no[2] = *(uint32_t *)0x0080A044;
	serial_no[3] = *(uint32_t *)0x0080A048;
	sprintf(device_serial_no, "%08x%08x%08x%08x",
			(unsigned)serial_no[0], (unsigned)serial_no[1],
			(unsigned)serial_no[2], (unsigned)serial_no[3]);
	black_system_init();
	enable_button_interrupt();
	printf(">>I11D %x %x %x %x \r\n", (unsigned int)serial_no[0],
			(unsigned int)serial_no[1], (unsigned int)serial_no[2], 
			(unsigned int)serial_no[3]);
	init_rtc();	
	/*while (true) {
		printf("enter sleeping\r\n");
		sleepmgr_enter_sleep();
		printf("leave sleeping\r\n");
		delay_s(5);
	}*/
	json = (char *)malloc(1286*sizeof(char));
	buf = (uint8_t *)malloc(550*sizeof(uint8_t));	
	xt_data_now = (uint8_t *)malloc(550*sizeof(uint8_t));
	send = (uint8_t *)malloc(1520*sizeof(uint8_t));
	imsi_str = (char *)malloc(20*sizeof(char));
	while (true) {
		get_rtc_time(&rtc_time_now);
		if (rtc_time_now.hour == g_hour && long_press)
		{
			printf("continue to sleep %d %d %d\r\n",
				rtc_time_now.hour,g_hour,long_press);
			at25dfx_off();
			sleepmgr_enter_sleep();
			continue;
		}
		at25dfx_on();
		long_press = true;
		g_hour = rtc_time_now.hour;
		uhc_start();
		delay_s(2);
		usb_power(1);
		delay_s(5);
		if (libre_found) {
			if (uhc_is_suspend())
				uhc_resume();
			delay_ms(100);
			if (!uhc_is_suspend()) {
				apollo_init();	
				if (strlen(cur_libre_serial_no) != 0) {
					if (cur_ts == -1 || bak_ts == -1 ||
							cur_gid == -1 || bak_gid == -1) {
						cur_ts = get_dev_ts(cur_libre_serial_no,strlen(cur_libre_serial_no));
						bak_ts = cur_ts;
						cur_gid = get_dev_gid();
						bak_gid = cur_gid;
						printf("%s inserted , ts %d, gid %d\r\n",
								cur_libre_serial_no,(int)cur_ts,(int)cur_gid);
					}
					printf("cur ts %d %d, cur gid %d %d\r\n", cur_ts,bak_ts,cur_gid,bak_gid);
					ui_init();
					if (cur_ts < bak_ts || cur_gid < bak_gid)
						update_time(server_time);
				}
				uhc_suspend(false);
			}
		} else
			printf("no sugar insert\r\n");
		uhc_stop(false);
		usb_power(0);
		gprs_power(0);
		at25dfx_off();
		sleepmgr_enter_sleep();
	}
}

