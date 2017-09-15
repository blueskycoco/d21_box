#include <asf.h>
#include "history.h"
#include "conf_at25dfx.h"
#define LEN_ALL_DEV_NUM 1
#define LEN_DEV_SERIAL 1
#define LEN_ADDR_OFFSET 3
uint32_t dev_addr = 0;

uint8_t dev_info[4096] = {0};
struct spi_module at25dfx_spi;
struct at25dfx_chip_module at25dfx_chip;
static void at25dfx_init(void)
{
	struct at25dfx_chip_config at25dfx_chip_config;
	struct spi_config at25dfx_spi_config;	

	at25dfx_spi_get_config_defaults(&at25dfx_spi_config);
	at25dfx_spi_config.mode_specific.master.baudrate = AT25DFX_CLOCK_SPEED;
	at25dfx_spi_config.mux_setting = AT25DFX_SPI_PINMUX_SETTING;
	at25dfx_spi_config.pinmux_pad0 = AT25DFX_SPI_PINMUX_PAD0;
	at25dfx_spi_config.pinmux_pad1 = AT25DFX_SPI_PINMUX_PAD1;
	at25dfx_spi_config.pinmux_pad2 = AT25DFX_SPI_PINMUX_PAD2;
	at25dfx_spi_config.pinmux_pad3 = AT25DFX_SPI_PINMUX_PAD3;

	spi_init(&at25dfx_spi, AT25DFX_SPI, &at25dfx_spi_config);
	spi_enable(&at25dfx_spi);
	at25dfx_chip_config.type = AT25DFX_MEM_TYPE;
	at25dfx_chip_config.cs_pin = AT25DFX_CS;

	at25dfx_chip_init(&at25dfx_chip, &at25dfx_spi, &at25dfx_chip_config);

}
uint32_t get_serial_offset(uint8_t *serial, uint8_t len)
{
	uint8_t serial_len = 0;
	uint32_t serial_offset = 0;
	uint32_t offset = 1;	
	int i =0, j=0;
	uint8_t found = 0;
	enum status_code ret = STATUS_OK;
	
	ret = at25dfx_chip_wake(&at25dfx_chip);
	if (ret != STATUS_OK) { printf("spi wake failed\r\n"); return;}
	
	ret = at25dfx_chip_read_buffer(&at25dfx_chip, 0x0000, dev_info, 4096);
	if (ret != STATUS_OK) { 
		printf("spi wake failed\r\n"); 
		at25dfx_chip_sleep(&at25dfx_chip); 
		return;
	}
	printf("%d sugar devices\r\n", dev_info[0]);
	if (dev_info[0] == 0) {
		dev_info[0] = 1;
		dev_info[1] = len;
		memcpy(dev_info + 2, serial,len);
		dev_info[2+len] = 0x00;
		dev_info[3+len] = 0x10;
		dev_info[4+len] = 0x00;
		serial_offset = 0x1000;
		printf("insert first dev, serial %s, to 0x1000\r\n", serial);
	} else {		
		for (i = 0; i < dev_info[0]; i++)
		{
			printf("dev %d \r\nserial no : ", i);
			serial_len = dev_info[offset];
			offset ++;
			for (j = offset; j < dev_info[offset+serial_len]; j++)
				printf("%c", dev_info[j]);
			if (len == serial_len) {
					if (memcmp(serial, dev_info+offset, len) == 0)
					{
						serial_offset = dev_info[offset+serial_len]<<16 
							| dev_info[offset+serial_len+1]<<8 
							| dev_info[offset+serial_len+2];
						printf("found our dev offset 0x%03x\r\n", serial_offset);
						found = 1;
						break;
					}
			}
			offset += serial_len;
			printf(" \r\naddr_offset: 0x%03x\r\n", 
					dev_info[offset]<<16 | dev_info[offset+1]<<8 
					| dev_info[offset+2]);
			offset +=3;
			printf("\r\n");
		}

		if (!found) {
			dev_info[0] +=1;
			dev_info[offset++] = len;			
			memcpy(dev_info + offset, serial,len);
			offset += len;
			serial_offset = dev_info[0] * 64 * 1024;
			dev_info[offset] = (serial_offset << 16)&0xff;
			dev_info[offset+1] = (serial_offset << 8)&0xff;
			dev_info[offset+2] = serial_offset&0xff;
			printf("insert %d dev, serial %s, to 0x%03x\r\n", dev_info[0], serial, serial_offset);
			ret = at25dfx_chip_set_sector_protect(&at25dfx_chip, serial_offset, false);
			if (ret != STATUS_OK) printf("sector protect failed %d\r\n", ret);
			ret = at25dfx_chip_erase_block(&at25dfx_chip, serial_offset, AT25DFX_BLOCK_SIZE_4KB);
			if (ret != STATUS_OK) printf("erase block failed %d\r\n", ret); 
		}
	}
	ret = at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x00000, false);
	if (ret != STATUS_OK) printf("sector protect failed %d\r\n", ret);
	for (i=0; i<4096/256; i++)
	{
		ret = at25dfx_chip_write_buffer(&at25dfx_chip, i*256, dev_info+256, 256);
		if (ret != STATUS_OK) printf("write buffer failed %d\r\n", ret);	
	}
	ret = at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	if (ret != STATUS_OK) printf("global sector protect failed %d\r\n", ret);
	at25dfx_chip_sleep(&at25dfx_chip); 
}
uint8_t history_init(void)
{
	uint8_t ret = 0;
	at25dfx_init();
	at25dfx_chip_wake(&at25dfx_chip);
	if (at25dfx_chip_check_presence(&at25dfx_chip) != STATUS_OK) 
		printf("spi flash not exist\r\n");
	else
	{
		printf("found spi flash\r\n");
		ret = 1;
	}
	at25dfx_chip_sleep(&at25dfx_chip);
}

void test_flash(void)
{
	enum status_code ret = at25dfx_chip_read_buffer(&at25dfx_chip, 0x0000, dev_info, 4096);
	if (ret != STATUS_OK) printf("read buffer failed %d\r\n", ret);
	ret = at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x00000, false);
	if (ret != STATUS_OK) printf("sector protect failed %d\r\n", ret);
	ret = at25dfx_chip_erase_block(&at25dfx_chip, 0x00000, AT25DFX_BLOCK_SIZE_4KB);
	if (ret != STATUS_OK) printf("erase block failed %d\r\n", ret);	
	for (i=0; i<4096; i++) 
		dev_info[i] = i%256;
	for (i=0; i<4096/256; i++)
	ret = at25dfx_chip_write_buffer(&at25dfx_chip, i*256, dev_info+256, 256);
	if (ret != STATUS_OK) printf("write buffer failed %d\r\n", ret);	
	memset(dev_info, 0, 4096);
	ret = at25dfx_chip_read_buffer(&at25dfx_chip, 0x0000, dev_info, 4096);
	if (ret != STATUS_OK) printf("read buffer2 failed failed %d\r\n", ret);	
	ret = at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	if (ret != STATUS_OK) printf("global sector protect failed %d\r\n", ret);
	ret = at25dfx_chip_sleep(&at25dfx_chip);
	if (ret != STATUS_OK) printf("chip sleep failed %d\r\n", ret);
	history_num = (dev_info[0]<<8) | dev_info[1];
	for (i=0; i<4096; i++) {
		printf("0x%02x\t", dev_info[i]);
		}
}
