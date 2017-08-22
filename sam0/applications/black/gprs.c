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
static uint8_t gprs_send_cmd(const uint8_t *cmd, int len)
{
	uint8_t rcv[2] = {0};
	usart_serial_write_packet(&gprs_uart_module, cmd, len);
	usart_serial_read_packet(&gprs_uart_module, rcv, 2);
	if (memcmp(rcv, "OK", 2) == 0 ||
			memcmp(rcv, "CO", 2) == 0)
			return 1;
	else
		printf("there is no response from m26 cmd %s\r\n",cmd);

	return 0;
}
uint8_t gprs_config(void)
{
	uint8_t result = 0;
	const uint8_t qifgcnt[] 	= "AT+QIFGCNT=0\n";
	const uint8_t qicsgp[] 	= "AT+QICSGP=1,\"CMNET\"\n";
	const uint8_t qiregapp[] 	= "AT+QIREGAPP\n";
	const uint8_t qiact[] 	= "AT+QIACT\n";
	/*const uint8_t qhttpurl[] 	= "AT+QHTTPURL=49,30\n";
	const uint8_t url[] = "http://stage.weitang.com/sgSugarRecor/upload_json\n";
	uint8_t qhttpurl[] 	= "AT+QHTTPURL=48,30\n";
	  uint8_t url[] = "http://www.boyibang.com/sgSugarRecor/upload_json\n";*/	  
	const uint8_t qhttpurl[] 	= "AT+QHTTPURL=56,30\n";
	const uint8_t url[] = "http://123.57.26.24:8080/saveData/airmessage/messMgr.do\n";
	gprs_init();

	result = gprs_send_cmd(qifgcnt, strlen((const char *)qifgcnt));
	if (result)
		result = gprs_send_cmd(qicsgp, strlen((const char *)qicsgp));
	if (result)
		result = gprs_send_cmd(qiregapp, strlen((const char *)qiregapp));
	if (result)
		result = gprs_send_cmd(qiact, strlen((const char *)qiact));
	if (result)
	{
		result = gprs_send_cmd(qhttpurl, strlen((const char *)qhttpurl));
		if (result)
		{
			uint8_t rcv[2] = {0};
			usart_serial_write_packet(&gprs_uart_module, url, strlen((const char *)url));
			usart_serial_read_packet(&gprs_uart_module, rcv, 2);
			if (memcmp(rcv, "OK", 2) == 0)
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
	uint8_t rcv[7] = {0};
	uint8_t response[16] = {0};
	const uint8_t read_response[] = "AT+QHTTPREAD=30\n";
	sprintf((char *)post_cmd, "AT+QHTTPPOST=%d,50,10\n", len);
	
	usart_serial_write_packet(&gprs_uart_module, post_cmd, strlen((const char *)post_cmd));
	usart_serial_read_packet(&gprs_uart_module, rcv, 7);

	if (memcmp(rcv, "CONNECT", 7) == 0)
	{		
		usart_serial_write_packet(&gprs_uart_module, data, len);
		usart_serial_read_packet(&gprs_uart_module, rcv, 2);
		if (memcmp(rcv, "OK", 2) == 0)
		{
			result = gprs_send_cmd(read_response, strlen((const char *)read_response));
			if (result)
			{
				usart_serial_read_packet(&gprs_uart_module, response, 16);
				printf("response: %s \r\n", response);
			}
			else
				printf("there is no response from m26 3\r\n");
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
	char data[] = "{\"0\":\"5\",\"30\":\"Radar048\",\"35\":\"\",\"36\":\"9517\"}";
	http_post(data,strlen(data));
}
