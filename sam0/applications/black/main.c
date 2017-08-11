/**
 * \file
 *
 * \brief SAM0 USB Host Mass Storage Bootloader Application with CRC Check.
 *
 * Copyright (C) 2014-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include <asf.h>
#include <string.h>
#include "conf_usb_host.h"
#include "conf_bootloader.h"
#if CONSOLE_OUTPUT_ENABLED
#include "conf_uart_serial.h"
#endif
#include "main.h"

/* File Data Buffer */
//COMPILER_WORD_ALIGNED
//volatile uint8_t buffer[FLASH_BUFFER_SIZE];
char device_serial_no[32] = {0};
#if CONSOLE_OUTPUT_ENABLED
//! Console Strings
#define APP_HEADER                   \
					"ATMEL SAM D21 Firmware Generator for USB MSC BOOTLOADER\r\n"
#define TASK_PASSED                  "PASS\r\n"
#define TASK_FAILED                  "FAIL\r\n"

struct usart_module usart_instance;

static void console_init(void);
#endif
static struct usart_module gprs_uart_module;

#if CONSOLE_OUTPUT_ENABLED
/**
 * \brief Initializes the console output
 */
static void console_init(void)
{
	struct usart_config usart_conf;
	static struct usart_module cdc_uart_module;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = CONF_STDIO_MUX_SETTING;
	usart_conf.pinmux_pad0 = CONF_STDIO_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = CONF_STDIO_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = CONF_STDIO_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = CONF_STDIO_PINMUX_PAD3;
	usart_conf.baudrate    = CONF_STDIO_BAUDRATE;

	stdio_serial_init(&cdc_uart_module, CONF_STDIO_USART_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
}
#endif

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

	return 0;
}
static uint8_t gprs_config(void)
{
	uint8_t result = 0;
	const uint8_t qifgcnt[] 	= "AT+QIFGCNT=0\n";
	const uint8_t qicsgp[] 	= "AT+QICSGP=1,\"CMNET\"\n";
	const uint8_t qiregapp[] 	= "AT+QIREGAPP\n";
	const uint8_t qiact[] 	= "AT+QIACT\n";
	const uint8_t qhttpurl[] 	= "AT+QHTTPURL=49,30\n";
	const uint8_t url[] = "http://stage.weitang.com/sgSugarRecor/upload_json\n";
	/*uint8_t qhttpurl[] 	= "AT+QHTTPURL=48,30\n";
	  uint8_t url[] = "http://www.boyibang.com/sgSugarRecor/upload_json\n";*/
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

static uint8_t http_post(uint8_t *data, int len)
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
				printf("response: %s \n", response);
			}
		}
	}
	
	return result;
}
/**
 * \brief Initializes the device for the bootloader
 */
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
	delay_ms(5);
	/* Print a header */
	puts(APP_HEADER);
#endif
	/* Enable the global interrupts */
	cpu_irq_enable();
	ui_init();
	/* Start USB host stack */
	uhc_start();
	gprs_config();
}


/**
 * \brief Main function. Execution starts here.
 */
int main(void)
{
	uint32_t serial_no[4];

	/* Device initialization for the bootloader */
	serial_no[0] = *(uint32_t *)0x0080A00C;
	serial_no[1] = *(uint32_t *)0x0080A040;
	serial_no[2] = *(uint32_t *)0x0080A044;
	serial_no[3] = *(uint32_t *)0x0080A048;
	sprintf(device_serial_no, "%08x%08x%08x%08x",
		(unsigned)serial_no[0], (unsigned)serial_no[1],
		(unsigned)serial_no[2], (unsigned)serial_no[3]);
	black_system_init();
	printf("we are here\n");
#if CONSOLE_OUTPUT_ENABLED
	/* Print a header */
	puts("Insert device\r\n");
#endif
	while (true) {
		sleepmgr_enter_sleep();
	}
}

