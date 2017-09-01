/**
 * \file
 *
 * \brief USB host Human Interface Device (HID) mouse driver.
 *
 * Copyright (C) 2011-2015 Atmel Corporation. All rights reserved.
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

#include "conf_usb_host.h"
#include "usb_protocol.h"
#include "uhd.h"
#include "uhc.h"
#include "uhi_hid_generic.h"
#include <string.h>
bool libre_found = false;
#ifdef USB_HOST_HUB_SUPPORT
# error USB HUB support is not implemented on UHI mouse
#endif

uint8_t send_cmd_device_id[64] = {
    0x05,0x00,0x2f,0x50,0x72,0x6f,0x67,0x72,0x61,0x6d,0x44,0x61,
    0x74,0x61,0x2f,0x41,0x62,0x62,0x6f,0x74,0x74,0x20,0x44,0x69,
    0x61,0x62,0x65,0x74,0x65,0x73,0x20,0x43,0x61,0x72,0x65,0x2f,
    0x6d,0x61,0x73,0x2e,0x66,0x72,0x65,0x65,0x73,0x74,0x79,0x6c,
    0x65,0x6c,0x69,0x62,0x72,0x65,0x2e,0x6c,0x6f,0x67,0x00,0x00,
    0x00,0x00,0x00,0x00};
uint8_t send_cmd_tsi1[64] = {
	0x0a,0x07,0x65,0x12,0x4f,0x46,0x7d,0x16,0x7d,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00};
uint8_t cur_libre_serial_no[32] = {0};
typedef struct {
	uhc_device_t *dev;
	usb_ep_t ep_in;
	uint8_t report_size;
	uint8_t *report;
}uhi_hid_generic_dev_t;

static uhi_hid_generic_dev_t uhi_hid_generic_dev = {
	.dev = NULL,
	.report = NULL,
	};

static void uhi_hid_generic_start_trans_report(usb_add_t add);
static void uhi_hid_generic_report_reception(
		usb_add_t add,
		usb_ep_t ep,
		uhd_trans_status_t status,
		iram_size_t nb_transfered);


uhc_enum_status_t uhi_hid_generic_install(uhc_device_t* dev)
{
	bool b_iface_supported;
	uint16_t conf_desc_lgt;
	usb_iface_desc_t *ptr_iface;

	if (uhi_hid_generic_dev.dev != NULL) {
		return UHC_ENUM_SOFTWARE_LIMIT; // Device already allocated
	}
	conf_desc_lgt = le16_to_cpu(dev->conf_desc->wTotalLength);
	ptr_iface = (usb_iface_desc_t*)dev->conf_desc;
	b_iface_supported = false;
	while(conf_desc_lgt) {
		switch (ptr_iface->bDescriptorType) {

		case USB_DT_INTERFACE:
			if ((ptr_iface->bInterfaceClass   == HID_CLASS)
			&& (ptr_iface->bInterfaceProtocol == HID_PROTOCOL_GENERIC/*HID_PROTOCOL_MOUSE*/) ) {
				printf("find hid generic insert,vid %x ,pid %x\r\n",dev->dev_desc.idVendor,
					dev->dev_desc.idProduct);
				// USB HID Mouse interface found
				// Start allocation endpoint(s)
				b_iface_supported = true;
			} else {
				// Stop allocation endpoint(s)
				b_iface_supported = false;
			}
			break;

		case USB_DT_ENDPOINT:
			//  Allocation of the endpoint
			if (!b_iface_supported) {
				break;
			}
			if (!uhd_ep_alloc(dev->address, (usb_ep_desc_t*)ptr_iface)) {
				return UHC_ENUM_HARDWARE_LIMIT; // Endpoint allocation fail
			}
			Assert(((usb_ep_desc_t*)ptr_iface)->bEndpointAddress & USB_EP_DIR_IN);
			uhi_hid_generic_dev.ep_in = ((usb_ep_desc_t*)ptr_iface)->bEndpointAddress;
			uhi_hid_generic_dev.report_size =
					le16_to_cpu(((usb_ep_desc_t*)ptr_iface)->wMaxPacketSize);
			uhi_hid_generic_dev.report = malloc(uhi_hid_generic_dev.report_size);
			if (uhi_hid_generic_dev.report == NULL) {
				Assert(false);
				return UHC_ENUM_MEMORY_LIMIT; // Internal RAM allocation fail
			}
			uhi_hid_generic_dev.dev = dev;
			// All endpoints of all interfaces supported allocated
			return UHC_ENUM_SUCCESS;

		default:
			// Ignore descriptor
			break;
		}
		Assert(conf_desc_lgt>=ptr_iface->bLength);
		conf_desc_lgt -= ptr_iface->bLength;
		ptr_iface = (usb_iface_desc_t*)((uint8_t*)ptr_iface + ptr_iface->bLength);
	}
	return UHC_ENUM_UNSUPPORTED; // No interface supported
}
static bool set_idle(uhc_device_t* dev)
{
	usb_setup_req_t req;
	req.bmRequestType = USB_REQ_RECIP_INTERFACE | USB_REQ_TYPE_CLASS | USB_REQ_DIR_OUT;
	req.bRequest = 0x0a;
	req.wValue = 0;
	req.wIndex = 0;
	req.wLength = 0;
	if (!uhd_setup_request(dev->address,
		&req,
		NULL,
		0,
		NULL, NULL)) {
		return false;
	}
	return true;
}
bool send_cmd(uhc_device_t *dev, const uint8_t *cmd)
{
	usb_setup_req_t req;
	req.bmRequestType = USB_REQ_RECIP_INTERFACE | USB_REQ_TYPE_CLASS | USB_REQ_DIR_OUT;
	req.bRequest = 0x09;
	req.wValue = 0x2000;
	req.wIndex = 0;
	req.wLength = uhi_hid_generic_dev.report_size;
	if (!uhd_setup_request(dev->address,
		&req,
		cmd,
		uhi_hid_generic_dev.report_size,
		NULL, NULL)) {
		return false;
	}
	return true;

}
bool get_cap_data()
{	
	usb_setup_req_t req;
	req.bmRequestType = USB_REQ_RECIP_INTERFACE | USB_REQ_TYPE_CLASS | USB_REQ_DIR_OUT;
	req.bRequest = 0x09;
	req.wValue = 0x2000;
	req.wIndex = 0;
	req.wLength = uhi_hid_generic_dev.report_size;
	if (!uhd_setup_request(uhi_hid_generic_dev.dev->address,
		&req,
		send_cmd_device_id,
		uhi_hid_generic_dev.report_size,
		NULL, NULL)) {
		printf("get cap data failed\r\n");	
		return false;
	}
	printf("get cap data done\r\n");	
	uhi_hid_generic_start_trans_report(uhi_hid_generic_dev.dev->address);
	return true;
}
void uhi_hid_generic_enable(uhc_device_t* dev)
{
	if (uhi_hid_generic_dev.dev != dev) {
		return;  // No interface to enable
	}

	set_idle(dev);	
	libre_found = true;
	//send_cmd(dev,send_cmd_device_id);
	//uhi_hid_generic_start_trans_report(dev->address);
}

void uhi_hid_generic_uninstall(uhc_device_t* dev)
{
	if (uhi_hid_generic_dev.dev != dev) {
		return; // Device not enabled in this interface
	}
	uhi_hid_generic_dev.dev = NULL;
	Assert(uhi_hid_generic_dev.report!=NULL);
	free(uhi_hid_generic_dev.report);
}
static void uhi_hid_generic_start_trans_report(usb_add_t add)
{
	// Start transfer on interrupt endpoint IN
	bool ret = uhd_ep_run(add, uhi_hid_generic_dev.ep_in, true, uhi_hid_generic_dev.report,
			uhi_hid_generic_dev.report_size, 0, uhi_hid_generic_report_reception);
	if (!ret)
		printf("uhd ep run failed\r\n");
}

static void uhi_hid_generic_report_reception(
		usb_add_t add,
		usb_ep_t ep,
		uhd_trans_status_t status,
		iram_size_t nb_transfered)
{
	uint8_t state_prev;
	uint8_t state_new;
	int i;
	UNUSED(ep);
	printf("uhi_hid_generic_report_reception ==> \r\nadd %x, ep %x, status %d, len %d\r\n",
		add,ep,status,nb_transfered);
	if ((status == UHD_TRANS_NOTRESPONDING) || (status == UHD_TRANS_TIMEOUT)) {
		uhi_hid_generic_start_trans_report(add);
		return; // HID mouse transfer restart
	}

	if ((status != UHD_TRANS_NOERROR) || (nb_transfered < 4)) {
		printf("nb transfered < 4 %d\r\n", status);
		//uhi_hid_generic_start_trans_report(add);
		return; // HID mouse transfer aborted
	}
	for (i = 0; i < uhi_hid_generic_dev.report_size; i++)
	{
		printf("%02x\t", uhi_hid_generic_dev.report[i]);
	}
	printf("\r\n");
	//add more code here to parse cap data, ts, database etc
	if (uhi_hid_generic_dev.report[0] == 0x06)
	{//cur libre serial no
		memcpy(cur_libre_serial_no, uhi_hid_generic_dev.report+2, uhi_hid_generic_dev.report[1]);
		printf("cur libre %s\r\n", cur_libre_serial_no);
		//libre_found = true;
	}
	else if(uhi_hid_generic_dev.report[0] == 0x07)
	{//cap data & ts
		printf("cap data , ts \r\n");
	}
	printf("uhi_hid_generic_report_reception <==\r\n");
	//uhi_hid_generic_start_trans_report(add);
}
