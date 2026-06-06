/**
 * dev-proteus - Hook Manager
 * Central entry point for all hook libraries
 *
 * This file handles:
 * - Initialization
 * - Connection to emulator
 * - Dispatching to specific hook handlers
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <fcntl.h>     // Required for O_CREAT, O_RDWR, O_WRONLY, etc.
#include <sys/stat.h>  // Required for file permission macros (e.g., S_IRUSR)
#include <unistd.h>    // Required for close(), read(), and write()

#include <dlfcn.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#include "common.h"
#include "proteus.h"

 //=============================================================================
 // Forward declarations for hook functions (defined in separate files)
 //=============================================================================

 // I2C (i2c_hook.c)
void* i2c_hook_open(const char* name, int flags, ...);
int i2c_hook_close(VirtualDevice* device);
ssize_t i2c_hook_read(VirtualDevice* device, void* buf, size_t len);
ssize_t i2c_hook_write(VirtualDevice* device, const void* buf, size_t count);
int i2c_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

// SPI (spi_hook.c)
void* spi_hook_open(const char* name, int flags, ...);
int spi_hook_close(VirtualDevice* device);
ssize_t spi_hook_read(VirtualDevice* device, void* buf, size_t len);
ssize_t spi_hook_write(VirtualDevice* device, const void* buf, size_t count);
int spi_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

// UART (uart_hook.c)
void* uart_hook_open(const char* name, int flags, ...);
int uart_hook_close(VirtualDevice* device);
ssize_t uart_hook_read(VirtualDevice* device, void* buf, size_t len);
ssize_t uart_hook_write(VirtualDevice* device, const void* buf, size_t count);
int uart_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

// GPIO (gpio_hook.c)
void* gpio_hook_open(const char* name, int flags, ...);
int gpio_hook_close(VirtualDevice* device);
ssize_t gpio_hook_read(VirtualDevice* device, void* buf, size_t len);
ssize_t gpio_hook_write(VirtualDevice* device, const void* buf, size_t count);
int gpio_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

//=============================================================================
// Real (Original) function pointers
//=============================================================================

typedef int (*open_func)(const char* name, int flags, ...);
typedef int (*close_func)(int fd);
typedef ssize_t(*read_func)(int fd, void* buf, size_t len);
typedef ssize_t(*write_func)(int fd, const void* buf, size_t count);
typedef int (*ioctl_func)(int fd, unsigned long request, ...);

//=============================================================================
// Internal data
//=============================================================================
proteus_config_t g_proteus_config = {
	.emulator_host = PROTEUS_HOST,
	.emulator_port = PROTEUS_PORT,
	.enable_i2c = PROTEUS_ENABLE_I2C,
	.enable_spi = PROTEUS_ENABLE_SPI,
	.enable_uart = PROTEUS_ENABLE_UART,
	.enable_gpio = PROTEUS_ENABLE_GPIO,
	.verbose_logging = PROTEUS_VERBOSE,
};

static open_func s_real_open = NULL;
static close_func s_real_close = NULL;
static read_func s_real_read = NULL;
static write_func s_real_write = NULL;
static ioctl_func s_real_ioctl = NULL;

static VirtualDevice* s_devices = NULL;
static pthread_mutex_t s_devices_mutex = PTHREAD_MUTEX_INITIALIZER;

//=============================================================================
// Internal functions
//=============================================================================
__attribute__((constructor))
void init_hook_manager()
{
	proteus_init();

	PROTEUS_LOG("Initialized");
}

__attribute__((destructor))
void cleanup_hook_manager()
{
	proteus_destroy();
	pthread_mutex_destroy(&s_devices_mutex);

	PROTEUS_LOG("Cleaned Up");
}

static void add_device(VirtualDevice* device)
{
	pthread_mutex_lock(&s_devices_mutex);

	if (s_devices == NULL)
	{
		s_devices = device;
	}
	else
	{
		device->next = s_devices;
		s_devices = device;
	}

	pthread_mutex_unlock(&s_devices_mutex);
}

static void remove_device(VirtualDevice* device)
{
	pthread_mutex_lock(&s_devices_mutex);

	VirtualDevice* prev = NULL;
	VirtualDevice* current = s_devices;

	while (current)
	{
		if (current == device)
		{
			VirtualDevice* next = current->next;
			if (prev == NULL)
			{
				s_devices = next;
			}
			else
			{
				prev->next = next;
			}

			break;
		}

		prev = current;
		current = current->next;
	}

	pthread_mutex_unlock(&s_devices_mutex);
}

static VirtualDevice* find_device_by_fd(int fd)
{
	VirtualDevice* result = NULL;

	pthread_mutex_lock(&s_devices_mutex);

	VirtualDevice* current = s_devices;
	while (current)
	{
		if (current->fd == fd)
		{
			result = current;
			break;
		}

		current = current->next;
	}

	pthread_mutex_unlock(&s_devices_mutex);

	return result;
}

//=============================================================================
// Hooked functions
//=============================================================================

int open(const char* name, int flags, ...)
{
	int resultFd = -1;

	mode_t mode = 0;
	if (flags & O_CREAT)
	{
		va_list args;
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	VirtualDevice* device = NULL;

	if (g_proteus_config.enable_i2c & (strncmp(name, "/dev/i2c-", 9) == 0))
	{
		device = (VirtualDevice*)i2c_hook_open(name, flags, mode);
	}

	if (device != NULL)
	{
		(void)strncpy(device->name, name, sizeof(device->name) - 1);

		// Save allocated virtual device
		add_device(device);
		resultFd = device->fd;
	}
	else
	{
		// Pass through to original open
		if (s_real_open == NULL)
		{
			s_real_open = (open_func)dlsym(RTLD_NEXT, "open");
		}

		resultFd = s_real_open(name, flags, mode);
	}

	return resultFd;
}

int close(int fd)
{
	int closeResult = -1;

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		remove_device(device);
		switch (device->type)
		{
		case DEV_TYPE_I2C_E:
			closeResult = i2c_hook_close(device);
			break;

		default:
			PROTEUS_LOG("Failed to close - device type (%d) not implemented", device->type);
			break;
		}
	}
	else
	{
		if (s_real_close == NULL)
		{
			s_real_close = (close_func)dlsym(RTLD_NEXT, "close");
		}

		closeResult = s_real_close(fd);
	}

	return closeResult;
}

ssize_t read(int fd, void* buf, size_t len)
{
	ssize_t readResult = -1;

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		switch (device->type)
		{
		case DEV_TYPE_I2C_E:
			readResult = i2c_hook_read(device, buf, len);
			break;

		default:
			PROTEUS_LOG("Failed to read - device type (%d) not implemented", device->type);
			break;
		}
	}
	else
	{
		if (s_real_read == NULL)
		{
			s_real_read = (read_func)dlsym(RTLD_NEXT, "read");
		}

		readResult = s_real_read(fd, buf, len);
	}

	return readResult;
}

ssize_t write(int fd, const void* buf, size_t count)
{
	ssize_t writeResult = -1;

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		switch (device->type)
		{
		case DEV_TYPE_I2C_E:
			writeResult = i2c_hook_write(device, buf, count);
			break;

		default:
			PROTEUS_LOG("Failed to write - device type (%d) not implemented", device->type);
			break;
		}
	}
	else
	{
		if (s_real_write == NULL)
		{
			s_real_write = (write_func)dlsym(RTLD_NEXT, "write");
		}

		writeResult = s_real_write(fd, buf, count);
	}

	return writeResult;
}

int ioctl(int fd, unsigned long request, ...)
{
	int ioctlResult = -1;

	va_list args;
	va_start(args, request);
	void* argp = va_arg(args, void*);
	va_end(args);

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		switch (device->type)
		{
		case DEV_TYPE_I2C_E:
			ioctlResult = i2c_hook_ioctl(device, request, argp);
			break;

		default:
			PROTEUS_LOG("Failed to ioctl - device type (%d) not implemented", device->type);
			break;
		}
	}
	else
	{
		if (s_real_ioctl == NULL)
		{
			s_real_ioctl = (ioctl_func)dlsym(RTLD_NEXT, "ioctl");
		}

		ioctlResult = s_real_ioctl(fd, request, argp);
	}

	return ioctlResult;
}