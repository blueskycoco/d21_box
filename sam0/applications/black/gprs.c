#include <asf.h>
#include <string.h>
#include "conf_uart_serial.h"
#include "gprs.h"
#include "misc.h"
#include "rtc_calendar.h"
#define MAX_TRY 3
static struct usart_module gprs_uart_module;
static bool gprs_status = false;
char *imsi_str = NULL;
struct usart_config usart_conf;
extern void ts2date(uint32_t time, struct rtc_calendar_time *date_out);
void gprs_init(void)
{
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
		usart_enable(&gprs_uart_module);
		port_pin_set_output_level(PIN_PA10, true);
		port_pin_set_output_level(PIN_PA11, false);
		delay_s(1);
		port_pin_set_output_level(PIN_PA11, true);
		delay_s(5);
	} else {
		gprs_status = false;
		usart_disable(&gprs_uart_module);
		port_pin_set_output_level(PIN_PA11, false);
		port_pin_set_output_level(PIN_PA10, false);
	}

}
static uint8_t gprs_send_cmd(const uint8_t *cmd, int len,int need_connect,uint8_t *rcv, uint16_t timeout)
{
	uint16_t rlen=0;
	uint32_t i = 0;
	uint8_t result = 0;
	memset(rcv,0,256);
	if (cmd!=NULL && len > 0)
		usart_serial_write_packet(&gprs_uart_module, cmd, len);
	while(1) {
		enum status_code ret = usart_serial_read_packet(&gprs_uart_module, rcv, 256, &rlen);
		if (rlen >= 2 && (ret == STATUS_OK || ret == STATUS_ERR_TIMEOUT)) {
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
			delay_us(1);
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
	return result;
}
#if 0

static uint8_t gprs_cmd(const uint8_t *cmd,uint8_t *rcv)
{
	uint16_t rlen=0;
	uint32_t i = 0;
	uint8_t result = 0;
	enum status_code ret;
	//printf("1gprs %s\r\n", cmd);
	memset(rcv,0,256);
	if (cmd!=NULL)
		usart_serial_write_packet(&gprs_uart_module, cmd, strlen(cmd));
	while(1) {
		ret = usart_serial_read_packet(&gprs_uart_module, rcv, 256, &rlen);
		if (rlen > 0 && (ret == STATUS_OK || ret == STATUS_ERR_TIMEOUT)) {			
				result = 1;
				break;			
		} else {
			delay_us(100);		
			i++;
			if (i >= 20 * 700) {
				printf("at cmd: %s ,timeout\r\n", cmd);
				break;
			}			
		}
		rlen=0;
		memset(rcv,0,256);
	}
	//printf("%d gprs %d rcv %s\r\n",ret, rlen,rcv);
	return result;
}

int test_baud(void)
{
	int baud = 75;
	uint8_t rcv[256] = {0};
	const uint8_t cmd1[] 	= "AT\n";
	while (1) {
		usart_disable(&gprs_uart_module);
		usart_conf.baudrate    = baud;
		printf("trying baud %d\r\n", baud);
		usart_serial_init(&gprs_uart_module, CONF_GPRS_USART_MODULE, &usart_conf);
		usart_enable(&gprs_uart_module);
		gprs_cmd(cmd1,rcv);
		if (strstr((const char *)rcv, "OK") != NULL)
			break;
		memset(rcv,0,256);		
		if (baud == 75)
			baud = 150;
		else if (baud == 150)
			baud = 300;
		else if (baud == 300)
			baud = 600;
		else if (baud == 600)
			baud = 1200;
		else if (baud == 1200)
			baud = 2400;
		else if (baud == 2400)
			baud = 4800;
		else if (baud == 4800)
			baud = 9600;
		else if (baud == 9600)
			baud = 14400;
		else if (baud == 14400)
			baud = 19200;
		else if (baud == 19200)
			baud = 28800;
		else if (baud == 28800)
			baud = 38400;
		else if (baud == 38400)
			baud = 57600;
		else if (baud == 57600)
			baud = 115200;
		else if (baud == 115200)
		{
			printf("fuck!\r\n");
			break;
		}
	}
	printf("baud is %d\r\n",baud);
	return baud;
}
void gprs_test(void)
{
	uint8_t rcv[256] = {0};
	const uint8_t cmd1[] 	= "AT+GSN\n";
	const uint8_t cmd2[] 	= "AT+CPIN?\n";
	const uint8_t cmd3[] 	= "AT+CIMI\n";
	const uint8_t cmd13[] 	= "AT+QCCID\n";
	const uint8_t cmd5[] 	= "AT+CREG?\n";
	const uint8_t cmd4[] 	= "AT+CSQ\n";
	const uint8_t cmd6[] 	= "AT+CGREG?\n";
	const uint8_t cmd7[] 	= "AT+COPS?\n";	
	const uint8_t cmd14[] 	= "AT+COPS=?\n";	
	const uint8_t cmd8[]	= "AT+QIFGCNT=0\n";
	const uint8_t cmd9[] 	= "AT+QICSGP=1,\"CMNET\"\n";
	const uint8_t cmd10[]   = "AT+QIREGAPP\n";
	const uint8_t cmd11[] 	= "AT+QISTAT\n";
	const uint8_t cmd12[] 	= "AT+QIACT\n";	
	const uint8_t qhttpcfg[]	= "AT+QHTTPCFG=\"Requestheader\",1\n";	
	const uint8_t qhttpurl[]	= "AT+QHTTPURL=67,30\n";
	const uint8_t url[] 		= "http://stage.boyibang.com/weitang/sgSugarRecord/xiaohei/upload_json\n";
	gprs_power(1);
	//delay_s(120);
	//test_baud();
	//gprs_cmd(cmd1,rcv);
	//gprs_cmd(cmd2,rcv);
	//gprs_cmd(cmd3,rcv);
	//gprs_cmd(cmd13,rcv);
	while(1) {
		gprs_cmd(cmd6,rcv);
		if (strstr((const char *)rcv, "+CGREG: 0,1") != NULL) 
			break;
		delay_s(1);
	}
	//gprs_cmd(cmd4,rcv);
	//gprs_cmd(cmd6,rcv);
	//gprs_cmd(cmd7,rcv);
	//gprs_cmd(cmd14,rcv);
	gprs_cmd(cmd8,rcv);
	gprs_cmd(cmd9,rcv);
	gprs_cmd(cmd10,rcv);
	gprs_cmd(cmd11,rcv);
	gprs_send_cmd(cmd12, strlen((const char *)cmd12),1,rcv,40);
	gprs_cmd(qhttpcfg,rcv);
	gprs_send_cmd(qhttpurl, strlen((const char *)qhttpurl),1,rcv,1);
	gprs_cmd(url,rcv);
	char data[] = "{\"data\": [{\"type\": 0,\"bloodSugar\": 6.8,\"actionTime\": 1502038862000,\"gid\": 1},{\"type\": 1,\"bloodSugar\": 6.9,\"actionTime\": 1502038862000,\"gid\": 2}],\"deviceId\": \"foduf3fdlfodf0dfdthffk\"}";
	memset(rcv,0,256);
	http_post((uint8_t *)data,strlen(data),rcv);
	if (strlen(rcv) != 0)
	{
		int i=0;
		printf("rcv %s\r\n",rcv);
		while(rcv[i] != '{')
			i++;
		printf("<SEND SERVER>\r\n%s\r\n<RCV>\r\n %s\r\n",
					data, rcv+i);
	}
}
uint8_t check_gprs(void)
{
	const uint8_t qistat[] 		= "AT+QISTAT\n";
	uint8_t rcv[256] = {0};
	if (gprs_send_cmd(qistat, strlen((const char *)qistat),0,rcv,1))
	{
		if (strstr((const char *)rcv, "IP GPRSACT") == NULL) 
			return 1;
		else
			printf("lost signal %s",
	}
	return 0;
}
#endif
uint8_t gprs_config(void)
{
	uint8_t result = 0;
	uint8_t rcv[256] = {0};
	const uint8_t cmd5[] 	= "AT+CGREG?\n";
	const uint8_t qifcimi[] 	= "AT+CIMI\n";
	const uint8_t qifgcnt[] 	= "AT+QIFGCNT=0\n";
	const uint8_t qicsgp[] 		= "AT+QICSGP=1,\"CMNET\"\n";
	const uint8_t qiregapp[] 	= "AT+QIREGAPP\n";
	const uint8_t qiact[] 		= "AT+QIACT\n";
	const uint8_t qistat[] 		= "AT+QISTAT\n";
	const uint8_t qhttpcfg[] 	= "AT+QHTTPCFG=\"Requestheader\",1\n";	
	const uint8_t qhttpurl[] 	= "AT+QHTTPURL=67,30\n";
	const uint8_t url[] 		= "http://stage.boyibang.com/weitang/sgSugarRecord/xiaohei/upload_json\n";
	if (gprs_status)
		return 0;
	while(1) {
		gprs_send_cmd(cmd5, strlen((const char *)cmd5),0,rcv,1);
		if (strstr((const char *)rcv, "+CGREG: 0,1") != NULL) 
			break;
		delay_s(1);
		
	}
	while (1) {
		gprs_send_cmd(qifcimi, strlen((const char *)qifcimi),0,rcv,1);
		printf("cimi %s\r\n", rcv);
		if (strstr(rcv , "CME ERROR") == NULL) {
		char *cimi = (char *)strstr(rcv,"CIMI");
		memset(imsi_str,'\0',20);
		if (cimi != NULL) {
			int i=6;
			while (i<strlen(cimi) && cimi[i] != '\r') {
				imsi_str[i-6] = cimi[i];
				i++;
			}
			printf("%s", imsi_str);		
			break;
		}
		}
		delay_s(1);		
	}
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
			memset(rcv,0,256);
			if (result)
				result = gprs_send_cmd(qiact, strlen((const char *)qiact),1,rcv,80);
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


uint8_t *send=NULL;
#define HTTP_HEADER "POST /weitang/sgSugarRecord/xiaohei/upload HTTP/1.1\r\nHOST: stage.boyibang.com\r\nAccept: */*\r\nUser-Agent: QUECTEL_MODULE\r\nConnection: Keep-Alive\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s"

static uint32_t http_post(uint8_t *data, int len)
{
	uint8_t result = 0;
	uint32_t time = 0;
	char rcv[256] = {0};
	uint8_t post_cmd[32] = {0};
	const uint8_t read_response[] = "AT+QHTTPREAD=30\n";
	memset(send,0,1520);
	sprintf((char *)send, HTTP_HEADER,len,data);
	sprintf((char *)post_cmd, "AT+QHTTPPOST=%d,50,10\n", strlen((const char *)send));
	strcat((char *)send,"\n");
	gprs_send_cmd(post_cmd, strlen((const char *)post_cmd),1,(uint8_t *)rcv,40);
	
	if (strstr((const char *)rcv, "CONNECT") != NULL) {		
		memset(rcv,0,256);
		//printf("%s",send);
		gprs_send_cmd(send,strlen((const char *)send),0,(uint8_t *)rcv,80);
		if (strstr((const char *)rcv, "OK") != NULL) {
			memset(rcv,0,256);
			result = gprs_send_cmd(read_response, strlen((const char *)read_response),0,(uint8_t *)rcv,1);
			if (result) {
				int i=0;
				while(rcv[i] != '{' && i<256)
					i++;
				printf("<RCV>\r\n%s\r\n",rcv+i);
				char *time_str = (char *)strstr(rcv+i, "systemTime");
				i=12;
				//memset(post_cmd, 0, 32);
				if (time_str != NULL) {
					while (	i<strlen(time_str) && time_str[i]!='}')
					{
						//post_cmd[i-2] = time_str[i];
						//printf("%c\r\n", time_str[i]);
						time = time*10 + time_str[i] - '0';
						i++;
					}
					printf("time %d\r\n", time);
				}
			}
			gprs_send_cmd(NULL,0,1,(uint8_t *)post_cmd,10);
		}
		else
			printf("there is no response from m26 2\r\n");
	}
	else
		printf("there is no response from m26 1\r\n");
	printf("rcv %s\r\n", rcv);
	return time;
}
uint8_t upload_data(char *json, uint32_t *time)
{
	struct rtc_calendar_time date_out;
	uint8_t result = 0;
	//char out[256] = {0};
	int n = 0,i = 0;
	while (n < MAX_TRY) {
		//printf("upload data\r\n%s\r\n",json);
		printf("<SEND SERVER>\r\n%s\r\n",json);
		//memset(out,0,256);
		*time = http_post((uint8_t *)json,strlen(json));
		//printf("upload data result %d, %s\r\n", result,out);
		if (*time != 0) {
			//i=0;
			//while(out[i] != '{' && i<256)
			//	i++;
			//ts2date(*time, &date_out);
			//if (i>=256)
			//	i=0;
			//printf("<SEND SERVER>\r\n%s\r\n<RCV>\r\n %s\r\n",
			//		json, out+i);
			result = 1;
			break;
		}
		n++;
	}
	return result;
}
