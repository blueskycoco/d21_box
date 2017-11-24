#include <asf.h>
#include "history.h"
#include "conf_at25dfx.h"
#define TS_LEN 4
uint32_t dev_addr = 0;

/*serial len 1 | serial n| ts 4 | gid 2*/
uint8_t dev_info[4096] = {0};
uint32_t g_index = 0;
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
void save_dev_ts(uint32_t ts, uint16_t gid)
{
	int i;
	if (g_index + 5 < 4096) {
		dev_info[g_index] = (ts >> 24) & 0xff;
		dev_info[g_index+1] = (ts >> 16) & 0xff;
		dev_info[g_index+2] = (ts >> 8) & 0xff;
		dev_info[g_index+3] = (ts >> 0) & 0xff;
		dev_info[g_index+4] = (gid >> 8) & 0xff;
		dev_info[g_index+5] = (gid ) & 0xff;
	}
	printf("save ts to %d %d %d\r\n", g_index,ts,gid);
	enum status_code ret = at25dfx_chip_wake(&at25dfx_chip);
	if (ret != STATUS_OK) {printf("chip wake failed %d\r\n", ret); return;}
	ret = at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x00000, false);
	if (ret != STATUS_OK) 
		printf("sector protect failed %d\r\n", ret);
	if (ret == STATUS_OK) 
		ret = at25dfx_chip_erase_block(&at25dfx_chip, 0x00000, 
				AT25DFX_BLOCK_SIZE_4KB);
	if (ret == STATUS_OK) 
		for (i=0; i<4096/256; i++) {
			ret = at25dfx_chip_write_buffer(&at25dfx_chip, i*256, 
											dev_info+i*256, 256);
			if (ret != STATUS_OK) printf("write buffer failed %d\r\n", ret);	
	}
	if (ret == STATUS_OK) {
		ret =at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
		if (ret != STATUS_OK) 
			printf("global sector protect failed %d\r\n", ret);
	}
	ret = at25dfx_chip_sleep(&at25dfx_chip);
	if (ret != STATUS_OK) printf("chip sleep failed %d\r\n", ret);
}
uint16_t g_gid = 0;
uint16_t get_dev_gid() {
	return g_gid;
}
uint32_t get_dev_ts(uint8_t *serial, uint8_t len)
{
	uint32_t ts = 0;
	bool found = false;
	int offset = 2;
	int dev_num = 0;
	int i,j;
	int devx_len = 0;
	enum status_code ret = at25dfx_chip_wake(&at25dfx_chip);
	if (ret != STATUS_OK) {printf("chip wake failed %d\r\n", ret); return ts;}

	ret = at25dfx_chip_read_buffer(&at25dfx_chip, 0x0000, dev_info, 4096);
	if (ret != STATUS_OK) {
		printf("read buffer failed %d\r\n", ret);
		at25dfx_chip_sleep(&at25dfx_chip);
		return ts;
	}
	dev_num = (dev_info[0] << 8) | dev_info[1];
	printf("dev_num %d, input %s , len %d\r\n", dev_num,serial,len);
	if (dev_num != 0xffff) {
		//int i = 0;
		for (i=0; i<dev_num; i++) {
			devx_len = dev_info[offset];
			printf("devx_len %d\r\n", devx_len);
			if (devx_len == len) {
				for (j=0;j<len;j++)
					printf("%c<>%c\r\n",serial[j],dev_info[offset+1+j]);
				if (memcmp(serial, &(dev_info[offset+1]), len) == 0) {
					found = true;
					ts = (dev_info[offset+len+1] << 24) |
						 (dev_info[offset+len+2] << 16) |
						 (dev_info[offset+len+3] << 8) |
						 (dev_info[offset+len+4] << 0);
					g_gid = (dev_info[offset+len+5]) << 8 |
							(dev_info[offset+len+6]) ;
					printf("found device offset %d , len %d, ts %d, gid %d\r\n", offset,dev_info[offset],(int)ts,(int)g_gid);
					g_index = offset+len+4;
					for (j=0; j<dev_info[offset]; j++)
						printf("%c", dev_info[offset+j+1]);
					printf("\r\n");
					break;
				}
			}				
			offset = offset + devx_len + TS_LEN + 2 +2;
			if (offset >= 4096) {
				at25dfx_chip_sleep(&at25dfx_chip);
				return 0;
			}
		}
	}
	if (!found) {
		if (dev_num == 0xffff) {/*first in use*/
			printf("this 222 spi flash first in use %d\r\n",len);
			dev_info[0] = 0x00;dev_info[1] = 0x01;
			dev_info[2] = len;
			memcpy(dev_info + 3, serial, len);
			dev_info[len+3] = 0x00;dev_info[len+4] = 0x00;
			dev_info[len+5] = 0x00;dev_info[len+6] = 0x00;
			dev_info[len+7] = 0x00;dev_info[len+8] = 0x00;
			g_index = len+3;
		} else {
			printf("add new device at %d \r\n",offset);
			dev_num++;
			dev_info[0] = (dev_num >> 8)&0xff;dev_info[1] = dev_num & 0xff;
			dev_info[offset] = len;
			memcpy(dev_info + offset + 1, serial, len);
			dev_info[offset+len+1] = 0x00;
			dev_info[offset+len+2] = 0x00;
			dev_info[offset+len+3] = 0x00;
			dev_info[offset+len+4] = 0x00;
			dev_info[offset+len+5] = 0x00;
			dev_info[offset+len+6] = 0x00;
			g_index = offset+len+1;
		}
		ret = at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x00000, false);
		if (ret != STATUS_OK) 
			printf("sector protect failed %d\r\n", ret);
		if (ret == STATUS_OK) 
			ret = at25dfx_chip_erase_block(&at25dfx_chip, 0x00000, 
					AT25DFX_BLOCK_SIZE_4KB);
		if (ret == STATUS_OK) 
			for (i=0; i<4096/256; i++) {
				ret = at25dfx_chip_write_buffer(&at25dfx_chip, i*256, 
												dev_info+i*256, 256);
				if (ret != STATUS_OK) printf("write buffer failed %d\r\n", ret);	
		}
		if (ret == STATUS_OK) {
			ret =at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
			if (ret != STATUS_OK) 
				printf("global sector protect failed %d\r\n", ret);
		}
	}
	ret = at25dfx_chip_sleep(&at25dfx_chip);
	if (ret != STATUS_OK) printf("chip sleep failed %d\r\n", ret);
	return ts;
}
uint8_t history_init(void)
{
	uint8_t ret = 0;
	at25dfx_init();
	at25dfx_chip_wake(&at25dfx_chip);
	if (at25dfx_chip_check_presence(&at25dfx_chip) != STATUS_OK) 
		printf("spi flash not exist\r\n");
	else {
		printf("found spi flash\r\n");
		ret = 1;
	}
	if (!port_pin_get_input_level(PIN_PA09)) {
		at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x00000, false);
		at25dfx_chip_erase_block(&at25dfx_chip, 0x00000, 
					AT25DFX_BLOCK_SIZE_4KB);
		at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	}
	at25dfx_chip_sleep(&at25dfx_chip);
	return ret;
}
/*
void test_flash(void)
{
	enum status_code ret = at25dfx_chip_read_buffer(&at25dfx_chip, 0x0000, 
			dev_info, 4096);
	if (ret != STATUS_OK) printf("read buffer failed %d\r\n", ret);
	ret = at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x00000, false);
	if (ret != STATUS_OK) printf("sector protect failed %d\r\n", ret);
	ret = at25dfx_chip_erase_block(&at25dfx_chip, 0x00000, 
			AT25DFX_BLOCK_SIZE_4KB);
	if (ret != STATUS_OK) printf("erase block failed %d\r\n", ret);	
	for (i=0; i<4096; i++) 
		dev_info[i] = i%256;
	for (i=0; i<4096/256; i++)
	ret = at25dfx_chip_write_buffer(&at25dfx_chip, i*256, dev_info+256, 256);
	if (ret != STATUS_OK) printf("write buffer failed %d\r\n", ret);	
	memset(dev_info, 0, 4096);
	ret = at25dfx_chip_read_buffer(&at25dfx_chip, 0x0000, dev_info, 4096);
	if (ret != STATUS_OK) printf("read buffer2 failed failed %d\r\n", ret);	
	ret =at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	if (ret != STATUS_OK) printf("global sector protect failed %d\r\n", ret);
	ret = at25dfx_chip_sleep(&at25dfx_chip);
	if (ret != STATUS_OK) printf("chip sleep failed %d\r\n", ret);
	history_num = (dev_info[0]<<8) | dev_info[1];
	for (i=0; i<4096; i++) {
		printf("0x%02x\t", dev_info[i]);
		}
}
*/
