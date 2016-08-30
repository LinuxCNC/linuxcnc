#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <endian.h>

#include "libsoc_mmap_gpio.h"

#define PIO_REG_SIZE 0x228 /*0x300*/
#define PIO_PORT_SIZE 0x24

#define PIO_REG_CFG(B, N, I)	((B) + (N)*0x24 + ((I)<<2) + 0x00)
#define PIO_REG_DLEVEL(B, N, I)	((B) + (N)*0x24 + ((I)<<2) + 0x14)
#define PIO_REG_PULL(B, N, I)	((B) + (N)*0x24 + ((I)<<2) + 0x1C)
#define PIO_REG_DATA(B, N)		((B) + (N)*0x24 + 0x10)
#define PIO_NR_PORTS			9 /* A-I */

#define LE32TOH(X)		le32toh(*((uint32_t*)(X)))

#define PIO_SUCCESS 0

static char* gpio_mem = NULL;

static int pio_get(const char* buf, mmap_gpio* pio)
{
	uint32_t port = pio->port - 'A';
	if (port > PIO_NR_PORTS)
	{
		return -1;
	}

	uint32_t val;
	uint32_t port_num_func, port_num_pull;
	uint32_t offset_func, offset_pull;

	port_num_func = pio->pin >> 3;
	offset_func = ((pio->pin & 0x07) << 2);

	port_num_pull = pio->pin >> 4;
	offset_pull = ((pio->pin & 0x0f) << 1);

	/* func */
	val = LE32TOH(PIO_REG_CFG(buf, port, port_num_func));
	pio->cfg = (val>>offset_func) & 0x07;

	/* pull */
	val = LE32TOH(PIO_REG_PULL(buf, port, port_num_pull));
	pio->pull = (val>>offset_pull) & 0x03;

	/* dlevel */
	val = LE32TOH(PIO_REG_DLEVEL(buf, port, port_num_pull));
	pio->drv_level = (val>>offset_pull) & 0x03;

	/* i/o data */
	if (pio->cfg > 1)
		pio->data = -1;
	else {
		val = LE32TOH(PIO_REG_DATA(buf, port));
		pio->data = (val >> pio->pin) & 0x01;
	}

	return PIO_SUCCESS;
}

static int pio_set(char* buf, mmap_gpio* pio)
{
	uint32_t port = pio->port - 'A';
	if (port > PIO_NR_PORTS)
	{
		return -1;
	}

	uint32_t *addr, val;
	uint32_t port_num_func, port_num_pull;
	uint32_t offset_func, offset_pull;

	port_num_func = pio->pin >> 3;
	offset_func = ((pio->pin & 0x07) << 2);

	port_num_pull = pio->pin >> 4;
	offset_pull = ((pio->pin & 0x0f) << 1);

	/* func */
	if (pio->cfg >= 0) {
		addr = (uint32_t*)PIO_REG_CFG(buf, port, port_num_func);
		val = le32toh(*addr);
		val &= ~(0x07 << offset_func);
		val |=  (pio->cfg & 0x07) << offset_func;
		*addr = htole32(val);
	}

	/* pull */
	if (pio->pull >= 0) {
		addr = (uint32_t*)PIO_REG_PULL(buf, port, port_num_pull);
		val = le32toh(*addr);
		val &= ~(0x03 << offset_pull);
		val |=  (pio->pull & 0x03) << offset_pull;
		*addr = htole32(val);
	}

	/* dlevel */
	if (pio->drv_level >= 0) {
		addr = (uint32_t*)PIO_REG_DLEVEL(buf, port, port_num_pull);
		val = le32toh(*addr);
		val &= ~(0x03 << offset_pull);
		val |=  (pio->drv_level & 0x03) << offset_pull;
		*addr = htole32(val);
	}

	/* data */
	if (pio->data >= 0) {
		addr = (uint32_t*)PIO_REG_DATA(buf, port);
		val = le32toh(*addr);
		if (pio->data)
			val |= (0x01 << pio->pin);
		else
			val &= ~(0x01 << pio->pin);
		*addr = htole32(val);
	}

	return PIO_SUCCESS;
}

int libsoc_mmap_gpio_init()
{
	if (gpio_mem != NULL)
	{
		return 0;
	}

	int ret = -1;

	int pagesize = sysconf(_SC_PAGESIZE);
	int addr = 0x01c20800 & ~(pagesize - 1);
	int offset = 0x01c20800 & (pagesize - 1);

	int fd = open("/dev/mem", O_RDWR);
	if (fd == -1) 
	{
		printf("Failed to open /dev/mem");
		goto clean;
	}

	gpio_mem = mmap(NULL, (0x800 + pagesize - 1) & ~(pagesize - 1), PROT_WRITE | PROT_READ, MAP_SHARED, fd, addr);
	if (gpio_mem == MAP_FAILED) 
	{
		printf("Failed to map GPIO");
		goto clean;
	}
	gpio_mem += offset;

	ret = 0;

clean:
	close(fd);
	return ret;
}

void libsoc_mmap_gpio_shutdown()
{
	gpio_mem = NULL;
}

mmap_gpio* libsoc_mmap_gpio_request(char port, unsigned int pin)
{
	mmap_gpio* gpio = calloc(sizeof(mmap_gpio), 1);
	*(char*)&gpio->port = port;
	*(int*)&gpio->pin = pin;
	if (pio_get(gpio_mem, gpio) == PIO_SUCCESS)
	{
		return gpio;
	}

	return NULL;
}

void libsoc_mmap_gpio_free(mmap_gpio* gpio)
{
	free(gpio);
}

int libsoc_mmap_gpio_set_direction(mmap_gpio* gpio, mmap_gpio_direction direction)
{
	if (gpio == NULL)
	{
		return DIRECTION_ERROR;
	}

	gpio->cfg = (direction == OUTPUT ? 1 : 0);
	if (pio_set(gpio_mem, gpio) == PIO_SUCCESS)
	{
		return direction;
	}

	return DIRECTION_ERROR;
}

mmap_gpio_direction libsoc_mmap_gpio_get_direction(mmap_gpio* gpio)
{
	if (gpio == NULL)
	{
		return DIRECTION_ERROR;
	}

	return (gpio->cfg == 1 ? OUTPUT : INPUT);
}

int libsoc_mmap_gpio_set_level(mmap_gpio* gpio, mmap_gpio_level level)
{
	if (gpio == NULL)
	{
		return LEVEL_ERROR;
	}

	gpio->data = (level == HIGH ? 1 : 0);
	if (pio_set(gpio_mem, gpio) == PIO_SUCCESS)
	{
		return level;
	}

	return LEVEL_ERROR;	
}

mmap_gpio_level libsoc_mmap_gpio_get_level(mmap_gpio* gpio)
{
	if (gpio == NULL)
	{
		return LEVEL_ERROR;
	}

	return (gpio->data & 0x1 ? HIGH : LOW);
}
