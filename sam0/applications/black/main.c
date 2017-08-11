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

#if CONSOLE_OUTPUT_ENABLED
//! Console Strings
#define APP_HEADER                   \
					"ATMEL SAM D21 Firmware Generator for USB MSC BOOTLOADER\r\n"
#define TASK_PASSED                  "PASS\r\n"
#define TASK_FAILED                  "FAIL\r\n"

struct usart_module usart_instance;

static void console_init(void);
#endif

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

static bool my_flag_cdc_available = false;
bool my_callback_cdc_change(uhc_device_t* dev, bool b_plug)
{
	if (b_plug) {
		my_flag_cdc_available = true;
		usb_cdc_line_coding_t cfg = {
			.dwDTERate = CPU_TO_LE32(115200),
			.bCharFormat = CDC_STOP_BITS_1,
			.bParityType = CDC_PAR_NONE,
			.bDataBits = 8,
		};
		uhi_cdc_open(0, &cfg);
	} else {
		my_flag_cdc_available = false;
	}
}
void my_callback_cdc_rx_notify(void)
{
	/* Wakeup my_task_rx() task */
}
#define MESSAGE "Hello"
void my_task(void)
{
	static bool startup = true;
	if (!my_flag_cdc_available) {
		startup = true;
		return;
	}
	if (startup) {
		startup = false;
		uhi_cdc_write_buf(0, MESSAGE, sizeof(MESSAGE)-1);
		uhi_cdc_putc(0,'\n');
		return;
	}
}

void my_task_rx(void)
{
	while (uhi_cdc_is_rx_ready(0)) {
		int value = uhi_cdc_getc(0);
	}
}
/**
 * \brief Initializes the device for the bootloader
 */
static void bootloader_system_init(void)
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

	/* Start USB host stack */
	uhc_start();
}


/**
 * \brief Main function. Execution starts here.
 */
int main(void)
{
	/* Device initialization for the bootloader */
	bootloader_system_init();

#if CONSOLE_OUTPUT_ENABLED
	/* Print a header */
	puts("Insert device\r\n");
#endif
}

