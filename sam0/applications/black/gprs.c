#include <asf.h>
#include <string.h>
#include "conf_uart_serial.h"
#include "gprs.h"
static struct usart_module gprs_uart_module;

static void gprs_init(void)
{
	struct usart_config usart_conf;

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
static uint8_t gprs_send_cmd(const uint8_t *cmd, int len,int need_connect,uint8_t *rcv)
{
	uint16_t rlen=0;
	usart_serial_write_packet(&gprs_uart_module, cmd, len);
	while(1)
	{
		enum status_code ret = usart_serial_read_packet(&gprs_uart_module, rcv, 256, &rlen);
		if (rlen > 0 && (ret == STATUS_OK || ret == STATUS_ERR_TIMEOUT))
		{
			if (need_connect)
			{
				if (strstr(rcv, "CONNECT") != NULL || strstr(rcv, "OK")!= NULL)
					break;					
				memset(rcv,0,256);
			}
			else
				break;
		}
		else
			delay_us(100);
	}
	printf("gprs %d rcv %s\r\n",rlen,rcv);
	return 1;
}
uint8_t gprs_config(void)
{
	uint8_t result = 0;
	uint8_t rcv[256] = {0};
	const uint8_t qifgcnt[] 	= "AT+QIFGCNT=0\n";
	const uint8_t qicsgp[] 		= "AT+QICSGP=1,\"CMNET\"\n";
	const uint8_t qiregapp[] 	= "AT+QIREGAPP\n";
	const uint8_t qiact[] 		= "AT+QIACT\n";
	const uint8_t qistat[] 		= "AT+QISTAT\n";
	const uint8_t qhttpcfg[] 	= "AT+QHTTPCFG=\"Requestheader\",1\n";	
	const uint8_t qhttpurl[] 	= "AT+QHTTPURL=67,30\n";
	const uint8_t qhttpurl_ask	= "AT+QHTTPURL?\n";
	const uint8_t url[] 		= "http://stage.boyibang.com/weitang/sgSugarRecord/xiaohei/upload_json\n";
	gprs_init();
	
	result = gprs_send_cmd(qistat, strlen((const char *)qistat),0,rcv);
	if (result) {
		if (strstr(rcv, "IP GPRSACT") == NULL) { 
			memset(rcv,0,256);
			result = gprs_send_cmd(qifgcnt, strlen((const char *)qifgcnt),0,rcv);
			memset(rcv,0,256);
			if (result)
				result = gprs_send_cmd(qicsgp, strlen((const char *)qicsgp),0,rcv);
			memset(rcv,0,256);
			if (result)
				result = gprs_send_cmd(qiregapp, strlen((const char *)qiregapp),0,rcv);
			memset(rcv,0,256);
			if (result)
				result = gprs_send_cmd(qiact, strlen((const char *)qiact),1,rcv);
			}
		else
			printf("gprs network already ok\r\n");
		}
	
	
		memset(rcv,0,256);
		if (result)
		{
			gprs_send_cmd(qhttpcfg, strlen((const char *)qhttpcfg),0,rcv);			
			result = gprs_send_cmd(qhttpurl, strlen((const char *)qhttpurl),1,rcv);
			if (result)
			{
				memset(rcv,0,256);
				gprs_send_cmd(url, strlen((const char *)url),0,rcv);
				if (strstr(rcv, "OK") != NULL)
					result = 1;
				else
					result = 0;
			}
		}	
	
	return result;
}

uint8_t http_post(uint8_t *data, int len)
{
	uint8_t result = 0;
	uint8_t post_cmd[32] = {0};
	uint8_t rcv[256] = {0};
	uint8_t send[400] = {0};
	uint8_t len_string[256] = {0};
	uint8_t http_header[] = "POST /weitang/sgSugarRecord/xiaohei/upload_json HTTP/1.1\r\nHOST: stage.boyibang.com\r\nAccept: */*\r\nUser-Agent: QUECTEL_MODULE\r\nConnection: Keep-Alive\r\nContent-Type: application/json\r\n";
	const uint8_t read_response[] = "AT+QHTTPREAD=30\n";
	strcpy(send,http_header);
	sprintf(len_string,"Content-Length: %d\r\n\r\n%s",len,data);
	strcat(send,len_string);
	sprintf((char *)post_cmd, "AT+QHTTPPOST=%d,50,10\n", strlen(send));
	//printf("send len %d\r\n",strlen(send));
	strcat(send,"\n");
	gprs_send_cmd(post_cmd, strlen((const char *)post_cmd),1,rcv);
	
	if (strstr(rcv, "CONNECT") != NULL)
	{		
		memset(rcv,0,256);
		//printf("%s",send);
		gprs_send_cmd(send,strlen(send),0,rcv);
		if (strstr(rcv, "OK") != NULL)
		{
			memset(rcv,0,256);
			result = gprs_send_cmd(read_response, strlen((const char *)read_response),0,rcv);			
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
	char data[] = "{\"data\": [{\"type\": 0,\"bloodSugar\": 6.8,\"actionTime\": 1502038862000,\"gid\": 1},{\"type\": 1,\"bloodSugar\": 6.9,\"actionTime\": 1502038862000,\"gid\": 2}],\"deviceId\": \"foduf3fdlfodf0dfdthffk\"}";
	http_post(data,strlen(data));
}
