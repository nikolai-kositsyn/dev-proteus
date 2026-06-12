/**
 * dev-proteus - Hook Manager
 * Central entry point for all hook functionality
 *
 * This file handles:
 * - Initialization / Destroying
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
 // Forward declarations for public hook functions
 //=============================================================================

 // I2C (i2c_hook.c)
void* i2c_hook_open(const char* name, int flags, ...);

// SPI (spi_hook.c)
void* spi_hook_open(const char* name, int flags, ...);

// UART (uart_hook.c)
void* uart_hook_open(const char* name, int flags, ...);

// GPIO (gpio_hook.c)
void* gpio_hook_open(const char* name, int flags, ...);

//=============================================================================
// Internal data
//=============================================================================

ProteusContext g_proteusCtx =
{
	// Server
	.emulatorHost = PROTEUS_HOST,
	.emulatorPort = PROTEUS_PORT,
	.emulatorTimeoutMs = PROTEUS_TIMEOUT_MS,
	.emulatorRetry = PROTEUS_RETRY,
	.emulatorDelayMs = PROTEUS_DELAY_MS,	
};

static VirtualDevice* s_devices = NULL;
static pthread_mutex_t s_devices_mutex = PTHREAD_MUTEX_INITIALIZER;

//=============================================================================
// Internal functions
//=============================================================================

__attribute__((constructor))
void init_hook_manager()
{
	proteus_init();

	// Function pointers to original system functions	
	g_proteusCtx.real_open = (open_func)dlsym(RTLD_NEXT, "open");
	g_proteusCtx.real_close = (close_func)dlsym(RTLD_NEXT, "close");
	g_proteusCtx.real_read = (read_func)dlsym(RTLD_NEXT, "read");
	g_proteusCtx.real_write = (write_func)dlsym(RTLD_NEXT, "write");
	g_proteusCtx.real_ioctl = (ioctl_func)dlsym(RTLD_NEXT, "ioctl");

	PROTEUS_LOG("Initialized");
}

__attribute__((destructor))
void destroy_hook_manager()
{
	proteus_destroy();

	pthread_mutex_destroy(&s_devices_mutex);

	PROTEUS_LOG("Destroyed");
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
// Common hook functions
//=============================================================================

int open(const char* name, int flags, ...)
{
	//PROTEUS_LOG("Enter to open, name=%s", name);

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

	if (strncmp(name, "/dev/i2c-", 9) == 0)
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
		// Pass through to original 'open'
		resultFd = g_proteusCtx.real_open(name, flags, mode);
	}

	return resultFd;
}

int close(int fd)
{
	//PROTEUS_LOG("Enter to close, fd=%d", fd);

	int closeResult = -1;

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		remove_device(device);
		closeResult = device->impl_close(device);
	}
	else
	{
		// Pass through to original 'close'
		closeResult = g_proteusCtx.real_close(fd);
	}

	return closeResult;
}

ssize_t read(int fd, void* buf, size_t len)
{
	ssize_t readResult = -1;

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		readResult = device->impl_read(device, buf, len);
	}
	else
	{
		// Pass through to original 'read'
		readResult = g_proteusCtx.real_read(fd, buf, len);
	}

	return readResult;
}

ssize_t write(int fd, const void* buf, size_t count)
{
	ssize_t writeResult = -1;

	VirtualDevice* device = find_device_by_fd(fd);
	if (device != NULL)
	{
		writeResult = device->impl_write(device, buf, count);
	}
	else
	{
		// Pass through to original 'write'
		writeResult = g_proteusCtx.real_write(fd, buf, count);
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
		ioctlResult = device->impl_ioctl(device, request, argp);
	}
	else
	{
		// Pass through to original 'ioctl'
		ioctlResult = g_proteusCtx.real_ioctl(fd, request, argp);
	}

	return ioctlResult;
}