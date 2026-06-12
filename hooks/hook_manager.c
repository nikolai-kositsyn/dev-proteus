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
// Internal data and function declarations
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

static int s_managerSock = -1;

static ProteusDevInfo* s_fetchedDevices = NULL;
static uint16_t s_fetchedDevicesCount = 0;

static VirtualDevice* s_devices = NULL;
static pthread_mutex_t s_devicesMutex = PTHREAD_MUTEX_INITIALIZER;

static int fetch_all_devices();

//=============================================================================
// Internal functions
//=============================================================================

__attribute__((constructor))
void init_hook_manager()
{
	// Set function pointers to original system functions
	g_proteusCtx.real_open = (open_func)dlsym(RTLD_NEXT, "open");
	g_proteusCtx.real_close = (close_func)dlsym(RTLD_NEXT, "close");
	g_proteusCtx.real_read = (read_func)dlsym(RTLD_NEXT, "read");
	g_proteusCtx.real_write = (write_func)dlsym(RTLD_NEXT, "write");
	g_proteusCtx.real_ioctl = (ioctl_func)dlsym(RTLD_NEXT, "ioctl");	

	// Connect to server and get devices to hook
	s_managerSock = proteus_connect();
	if (s_managerSock >= 0)
	{
		(void)fetch_all_devices();
	}

	PROTEUS_LOG("Initialized");
}

__attribute__((destructor))
void destroy_hook_manager()
{
	// Destroy fetched devices
	if (s_fetchedDevices != NULL)
	{
		free((void*)s_fetchedDevices);
		s_fetchedDevicesCount = 0;
	}
	
	// Destroy virtual devices
	pthread_mutex_lock(&s_devicesMutex);

	while (s_devices)
	{
		VirtualDevice* next = s_devices->next;
		s_devices->impl_close(s_devices);

		s_devices = next;
	}

	pthread_mutex_unlock(&s_devicesMutex);
	pthread_mutex_destroy(&s_devicesMutex);

	// Destroy internal client
	if (s_managerSock >= 0)
	{
		proteus_disconnect(s_managerSock);
		s_managerSock = -1;
	}

	proteus_destroy();

	PROTEUS_LOG("Destroyed");
}

//=============================================================================
// Fetch devices management
//=============================================================================

static int fetch_all_devices()
{
	uint8_t payload[PROTEUS_MAX_PAYLOAD];
	uint16_t reqPayloadLen = 0;

	// Run transaction
	uint16_t sequence = proteus_next_sequence();

	if (proteus_send_request(s_managerSock, PROTEUS_CMD_GET_DEVICES, sequence, payload, reqPayloadLen) < 0)
	{
		return -1;
	}
	
	uint16_t respPayloadLen = 0;
	if(proteus_recv_response(s_managerSock, sequence, payload, sizeof(payload), &respPayloadLen) < 0)
	{
		return -1;
	}

	if (respPayloadLen > sizeof(s_fetchedDevicesCount))
	{
		uint16_t respPayloadOffset = 0;
		
		// Get count of devices
		memcpy(&s_fetchedDevicesCount, payload + respPayloadOffset, sizeof(s_fetchedDevicesCount));
		respPayloadOffset += sizeof(s_fetchedDevicesCount);		
		
		// Get devices info
		s_fetchedDevices = (ProteusDevInfo*)malloc(s_fetchedDevicesCount * sizeof(ProteusDevInfo));
		if (s_fetchedDevices == NULL)
		{
			PROTEUS_LOG("Failed to malloc when fetch devices");
			s_fetchedDevicesCount = 0;
			return -1;
		}

		PROTEUS_LOG("Fetched %u devices:", s_fetchedDevicesCount);

		for (uint16_t idx = 0; idx < s_fetchedDevicesCount; ++idx)
		{
			// Fill device info
			ProteusDevInfo* devInfo = &s_fetchedDevices[idx];
			memcpy(devInfo, payload + respPayloadOffset, sizeof(ProteusDevInfo));
			respPayloadOffset += sizeof(ProteusDevInfo);

			const char* typeStr = "UNKNOWN";
			switch (devInfo->type)
			{
				case DEV_TYPE_I2C_E: typeStr = "I2C"; break;
				case DEV_TYPE_SPI_E: typeStr = "SPI"; break;
				case DEV_TYPE_UART_E: typeStr = "UART"; break;
				case DEV_TYPE_GPIO_E: typeStr = "GPIO"; break;
			}

			PROTEUS_LOG("  [%u] %s - %s", idx, devInfo->name, typeStr);
		}
	}

	return 0;
}

static ProteusDevInfo* find_in_fetched_devices(const char* path)
{	 
    for (uint16_t idx = 0; idx < s_fetchedDevicesCount; ++idx)
	{
		ProteusDevInfo* devInfo = &s_fetchedDevices[idx];
        if (strcmp(path, devInfo->name) == 0)
		{			
            return devInfo;
        }
    }

    return NULL;
}

//=============================================================================
// Virtual devices management
//=============================================================================

static void add_device(VirtualDevice* device)
{
	pthread_mutex_lock(&s_devicesMutex);

	if (s_devices == NULL)
	{
		device->next = NULL;		
	}
	else
	{
		device->next = s_devices;		
	}

	s_devices = device;

	pthread_mutex_unlock(&s_devicesMutex);
}

static void remove_device(VirtualDevice* device)
{
	pthread_mutex_lock(&s_devicesMutex);

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

	pthread_mutex_unlock(&s_devicesMutex);
}

static VirtualDevice* find_device_by_fd(int fd)
{
	VirtualDevice* result = NULL;

	pthread_mutex_lock(&s_devicesMutex);

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

	pthread_mutex_unlock(&s_devicesMutex);

	return result;
}

//=============================================================================
// Common hook functions
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

	ProteusDevInfo* devInfo = find_in_fetched_devices(name);
	if (devInfo != NULL)
	{
		switch (devInfo->type)
		{
			case DEV_TYPE_I2C_E: 
				device = (VirtualDevice*)i2c_hook_open(name, flags, mode);
				break;
			case DEV_TYPE_SPI_E: 
				device = (VirtualDevice*)spi_hook_open(name, flags, mode);
				break;
			case DEV_TYPE_UART_E:
				device = (VirtualDevice*)uart_hook_open(name, flags, mode);
				break;
			case DEV_TYPE_GPIO_E:
				device = (VirtualDevice*)gpio_hook_open(name, flags, mode);
				break;
		}
	}

	if (device != NULL)
	{
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