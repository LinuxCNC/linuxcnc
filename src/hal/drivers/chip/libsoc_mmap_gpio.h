#ifndef _LIBSOC_MMAP_GPIO_H_
#define _LIBSOC_MMAP_GPIO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \struct mmap_gpio
 * \brief representation of an pointers to the port registers
 * \param int cfg - the configure register
 * \param int pull - the pull register
 * \param int drv_level - the multi-driven register
 * \param int data - data register
 * \param char port - port letter
 * \param int pin - port bit
 */

typedef struct {
	int cfg;
	int pull;
	int drv_level;
	int data;
	const char port;
	const unsigned int pin;
} mmap_gpio;

/**
 * \struct mmap_gpio_direction
 * \brief defined values for input/output direction
 */

typedef enum {
	DIRECTION_ERROR = -1,
	INPUT = 0,
	OUTPUT = 1,
} mmap_gpio_direction;

/**
 * \struct mmap_gpio_direction
 * \brief defined values for high/low gpio level
 */

typedef enum {
	LEVEL_ERROR = -1,
	LOW = 0,
	HIGH = 1,
} mmap_gpio_level;

/**
 * \fn int libsoc_mmap_gpio_init
 * \brief initialize mmap gpio, call it once before using gpio
 */

int libsoc_mmap_gpio_init();

/**
 * \fn void libsoc_mmap_gpio_shutdown
 * \brief shutdown mmap gpio
 */

void libsoc_mmap_gpio_shutdown();

/**
 * \fn mmap_gpio* libsoc_mmap_gpio_request(char port, unsigned int pin)
 * \brief request a gpio to use
 * \param char port - the port name ('A', 'B, 'C', ..)
 * \param unsigned int pin - the pin of port
 * \return pointer to mmap_gpio*
 */

mmap_gpio* libsoc_mmap_gpio_request(char port, unsigned int pin);

/**
 * \fn void libsoc_mmap_gpio_free(mmap_gpio* gpio)
 * \brief free a previously requested gpio
 * \param mmap_gpio* gpio - valid pointer to a requested gpio
 */

void libsoc_mmap_gpio_free(mmap_gpio* gpio);

/**
 * \fn int libsoc_mmap_gpio_set_direction(mmap_gpio* gpio, mmap_gpio_direction direction)
 * \brief set gpio to input or output
 * \param mmap_gpio* gpio - pointer to gpio struct on which to set the direction
 * \param mmap_gpio_direction direction - enumerated direction, INPUT or OUTPUT
 * \return current setuped direction or DIRECTION_ERROR if fail
 */

int libsoc_mmap_gpio_set_direction(mmap_gpio* gpio, mmap_gpio_direction direction);

/**
 * \fn mmap_gpio_direction libsoc_mmap_gpio_get_direction(mmap_gpio* gpio)
 * \brief get the current direction of the gpio
 * \param mmap_gpio* gpio - pointer to gpio struct on which to get the direction
 * \return current gpio direction or DIRECTION_ERROR if fail
 */

mmap_gpio_direction libsoc_mmap_gpio_get_direction(mmap_gpio* gpio);

/**
 * \fn int libsoc_mmap_gpio_set_level(mmap_gpio* gpio, mmap_gpio_level level)
 * \brief set the gpio level to high or low
 * \param mmap_gpio* gpio - pointer to gpio struct on which to set the level
 * \param gpio_level level - enumerated mmap_gpio_level, HIGH or LOW
 * \return current level of LEVEL_ERROR in case of fail
 */

int libsoc_mmap_gpio_set_level(mmap_gpio* gpio, mmap_gpio_level level);

/**
 * \fn mmap_gpio_level libsoc_mmap_gpio_get_level(mmap_gpio* gpio)
 * \brief gets the current gpio level
 * \param mmap_gpio* gpio - pointer to gpio struct on which to get the level
 * \return current level of LEVEL_ERROR in case of fail
 */

mmap_gpio_level libsoc_mmap_gpio_get_level(mmap_gpio* gpio);

#ifdef __cplusplus
}
#endif
#endif // _LIBSOC_MMAP_GPIO_H_