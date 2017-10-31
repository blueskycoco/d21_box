#include <asf.h>
#include <string.h>
#include "conf_uart_serial.h"
#include "gprs.h"
#include "misc.h"
#include "rtc_calendar.h"
#define MAX_TRY 3
static struct usart_module gprs_uart_module;
static bool gprs_status = false;
extern void ts2date(uint32_t time, struct rtc_calendar_time *date_out);
void gprs_init(void)
{
	struct usart_config usart_conf;
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	/* Configure LEDs as outputs, turn them off */
	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(PIN_PA11, &pin_conf);
	port_pin_set_output_level(PIN_PA11, false);
	port_pin_set_config(PIN_PA07, &pin_conf);
	port_pin_set_output_level(PIN_PA07, true);
	port_pin_set_config(PIN_PA10, &pin_conf);
	port_pin_set_output_level(PIN_PA10, false);
	port_pin_set_config(PIN_PA27, &pin_conf);
	port_pin_set_output_level(PIN_PA27, false);

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = CONF_GPRS_MUX_SETTING;
	usart_conf.pinmux_pad0 = CONF_GPRS_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = CONF_GPRS_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = CONF_GPRS_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = CONF_GPRS_PINMUX_PAD3;
	usart_conf.baudrate    = CONF_GPRS_BAUDRATE;

	usart_serial_init(&gprs_uart_module, CONF_GPRS_USART_MODULE, &usart_conf);
	usart_enable(&gprs_uart_module);
}

void gprs_power(int on)
{
	if (on)
	{
		if (gprs_status)
			return;
		port_pin_set_output_level(PIN_PA10, true);
		port_pin_set_output_level(PIN_PA11, false);
		delay_s(1);
		port_pin_set_output_level(PIN_PA11, true);
		delay_s(5);
	} else {
		gprs_status = false;
		port_pin_set_output_level(PIN_PA11, false);
		port_pin_set_output_level(PIN_PA10, false);
	}

}
static uint8_t gprs_send_cmd(const uint8_t *cmd, int len,int need_connect,uint8_t *rcv, uint16_t timeout)
{
	uint16_t rlen=0;
	uint32_t i = 0;
	uint8_t result = 0;
	printf("gprs %s\r\n", cmd);
	if (cmd!=NULL && len > 0)
		usart_serial_write_packet(&gprs_uart_module, cmd, len);
	while(1) {
		enum status_code ret = usart_serial_read_packet(&gprs_uart_module, rcv, 256, &rlen);
		//printf("%d %d %d\r\n",ret,rlen,i);
		if (rlen > 0 && (ret == STATUS_OK || ret == STATUS_ERR_TIMEOUT)) {
			//printf("%s\r\n", rcv);
			if (need_connect) {
				if (strstr((const char *)rcv, "CONNECT") != NULL || strstr((const char *)rcv, "OK")!= NULL
					||strstr((const char *)rcv, "ERROR")!= NULL) {
					result = 1;
					break;					
				}
				memset(rcv,0,256);
			} else {
				result = 1;
				break;
			}
		} else {
			//printf("gprs %d uart timeout %d\r\n",timeout,i);
			delay_us(100);
			if (timeout != 0) {
				i++;
				if (i >= timeout * 70) {
					printf("at cmd: %s ,timeout\r\n", cmd);
					break;
				}
			}
		}
		rlen=0;
	}
	printf("gprs %d rcv %s\r\n",rlen,rcv);
	return result;
}
uint8_t gprs_config(void)
{
	uint8_t result = 0;
	uint8_t rcv[256] = {0};
	const uint8_t qifcsq[] 	= "AT+CSQ\n";
	const uint8_t qifcimi[] 	= "AT+CIMI\n";
	const uint8_t qifgcnt[] 	= "AT+QIFGCNT=0\n";
	const uint8_t qicsgp[] 		= "AT+QICSGP=1,\"CMNET\"\n";
	const uint8_t qiregapp[] 	= "AT+QIREGAPP\n";
	const uint8_t qiact[] 		= "AT+QIACT\n";
	const uint8_t qistat[] 		= "AT+QISTAT\n";
	const uint8_t qhttpcfg[] 	= "AT+QHTTPCFG=\"Requestheader\",1\n";	
	const uint8_t qhttpurl[] 	= "AT+QHTTPURL=67,30\n";
//	const uint8_t qhttpurl_ask[]= "AT+QHTTPURL?\n";
	const uint8_t url[] 		= "http://stage.boyibang.com/weitang/sgSugarRecord/xiaohei/upload_json\n";
	if (gprs_status)
		return 0;
	
	result = gprs_send_cmd(qistat, strlen((const char *)qistat),0,rcv,1);
	if (result) {
		if (strstr((const char *)rcv, "IP GPRSACT") == NULL) { 
			memset(rcv,0,256);
			result = gprs_send_cmd(qifgcnt, strlen((const char *)qifgcnt),0,rcv,1);
			memset(rcv,0,256);
			if (result)
				result = gprs_send_cmd(qicsgp, strlen((const char *)qicsgp),0,rcv,1);
			memset(rcv,0,256);
			if (result)
				result = gprs_send_cmd(qiregapp, strlen((const char *)qiregapp),0,rcv,1);
			//gprs_send_cmd(qifcsq, strlen((const char *)qifcsq),0,rcv,1);
			//gprs_send_cmd(qifcimi, strlen((const char *)qifcimi),0,rcv,1);
			memset(rcv,0,256);
			while(1) {
				result = gprs_send_cmd(qistat, strlen((const char *)qistat),0,rcv,1);
				if (strstr((const char *)rcv, "IP START") != NULL)
					break;
				delay_s(1);
				}
			if (result)
				result = gprs_send_cmd(qiact, strlen((const char *)qiact),1,rcv,20);
			}
		else
			printf("gprs network already ok\r\n");
		}
	
	
		memset(rcv,0,256);
		if (result) {
			gprs_send_cmd(qhttpcfg, strlen((const char *)qhttpcfg),0,rcv,1);			
			result = gprs_send_cmd(qhttpurl, strlen((const char *)qhttpurl),1,rcv,1);
			if (result) {
				memset(rcv,0,256);
				gprs_send_cmd(url, strlen((const char *)url),0,rcv,1);
				if (strstr((const char *)rcv, "OK") != NULL)
					result = 1;
				else
					result = 0;
			}
		}	
	gprs_status = true;	
	return result;
}

uint8_t http_post(uint8_t *data, int len, char *rcv)
{
	uint8_t result = 0;
	uint8_t post_cmd[32] = {0};
	uint8_t send[2048] = {0};
	uint8_t len_string[1400] = {0};
	uint8_t http_header[] = "POST /weitang/sgSugarRecord/xiaohei/upload_json HTTP/1.1\r\nHOST: stage.boyibang.com\r\nAccept: */*\r\nUser-Agent: QUECTEL_MODULE\r\nConnection: Keep-Alive\r\nContent-Type: application/json\r\n";
	const uint8_t read_response[] = "AT+QHTTPREAD=30\n";
	strcpy((char *)send,(const char *)http_header);
	sprintf((char *)len_string,"Content-Length: %d\r\n\r\n%s",len,data);
	strcat((char *)send,(const char *)len_string);
	sprintf((char *)post_cmd, "AT+QHTTPPOST=%d,50,10\n", strlen((const char *)send));
	strcat((char *)send,"\n");
	gprs_send_cmd(post_cmd, strlen((const char *)post_cmd),1,(uint8_t *)rcv,20);
	
	if (strstr((const char *)rcv, "CONNECT") != NULL) {		
		memset(rcv,0,256);
		//printf("%s",send);
		gprs_send_cmd(send,strlen((const char *)send),0,(uint8_t *)rcv,20);
		if (strstr((const char *)rcv, "OK") != NULL) {
			memset(rcv,0,256);
			result = gprs_send_cmd(read_response, strlen((const char *)read_response),0,(uint8_t *)rcv,1);
			gprs_send_cmd(NULL,0,1,(uint8_t *)rcv,10);
		}
		else
			printf("there is no response from m26 2\r\n");
	}
	else
		printf("there is no response from m26 1\r\n");
	
	return result;
}
void test_gprs(void)
{
	//{"0":"5","30":"Radar048","35":"","36":"9517"}
	//"http://123.57.26.24:8080/saveData/airmessage/messMgr.do"
	//char data[] = "{\"0\":\"5\",\"30\":\"Radar048\",\"35\":\"\",\"36\":\"9517\"}";
//	char data[] = "{\r\n\"data\": [\r\n{\r\n\"type\": 0,\r\n\"bloodSugar\": 6.8,\r\n\"actionTime\": 1502038862000,\r\n\"gid\": 1\r\n},\r\n{\r\n\"type\": 1,\r\n\"bloodSugar\": 6.9,\r\n\"actionTime\": 1502038862000,\r\n\"gid\": 2\r\n}\r\n],\r\n\"deviceId\": \"foduf3fdlfodf0dfdthffk\"\r\n}\n";
	char out[256] = {0};
	char data[] = "{\"data\": [{\"type\": 0,\"bloodSugar\": 6.8,\"actionTime\": 1502038862000,\"gid\": 1},{\"type\": 1,\"bloodSugar\": 6.9,\"actionTime\": 1502038862000,\"gid\": 2}],\"deviceId\": \"foduf3fdlfodf0dfdthffk\"}";
	http_post((uint8_t *)data,strlen(data),out);
	if (strlen(out) != 0)
		printf("http post response %s\r\n",out);
}
uint8_t upload_data(char *json, uint32_t *time)
{
	struct rtc_calendar_time date_out;
	uint8_t result = 0;
	char out[256] = {0};
	int n = 0,i = 0;
	while (n < MAX_TRY) {
		//printf("upload data\r\n%s\r\n",json);
		memset(out,0,256);
		result = http_post((uint8_t *)json,strlen(json)
									,out);
		if (result) {
			i=0;
			while(out[i] != '{')
				i++;
			do_it((uint8_t *)out+i, time);
			ts2date(*time, &date_out);
			printf("<SEND SERVER>\r\n%s\r\n<RCV>\r\n %s\r\n<SERVER TIME> %4d-%02d-%02d %02d:%02d:%02d\r\n",
					json, out+i, date_out.year, date_out.month, date_out.day, date_out.hour, date_out.minute, date_out.second);
			break;
		}
		n++;
	}
	return result;
}
