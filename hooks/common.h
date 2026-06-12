/**
 * dev-proteus - Universal Linux Peripheral Emulator
 */

#ifndef PROTEUS_COMMON_H
#define PROTEUS_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <linux/limits.h> // NAME_MAX

#include "config.h"

 //=============================================================================
 // Base Virtual Device
 //=============================================================================

typedef enum DevTypeE
{
	DEV_TYPE_I2C_E = 0,
	DEV_TYPE_SPI_E,
	DEV_TYPE_UART_E,
	DEV_TYPE_GPIO_E,

	DEV_TYPE_MAX_E, // Keep it last
}DevTypeEnum;

typedef struct VirtualDeviceS
{
	DevTypeEnum type;
	int fd;
	char name[NAME_MAX];
	uint32_t id;

	int clientSock;

	int (*impl_close)(struct VirtualDeviceS* device);
	ssize_t(*impl_read)(struct VirtualDeviceS* device, void* buf, size_t len);
	ssize_t(*impl_write)(struct VirtualDeviceS* device, const void* buf, size_t count);
	int (*impl_ioctl)(struct VirtualDeviceS* device, unsigned long request, void* argp);

	struct VirtualDeviceS* next;
} VirtualDevice;

//=============================================================================
// Context
//=============================================================================

// Function pointers to original system functions
typedef int (*open_func)(const char* name, int flags, ...);
typedef int (*close_func)(int fd);
typedef ssize_t(*read_func)(int fd, void* buf, size_t len);
typedef ssize_t(*write_func)(int fd, const void* buf, size_t count);
typedef int (*ioctl_func)(int fd, unsigned long request, ...);

typedef struct ProteusContextS
{
	// Server
	char emulatorHost[NAME_MAX];
	int emulatorPort;
	int emulatorTimeoutMs;
	int emulatorRetry;
	int emulatorDelayMs;

	// Original functions
	open_func real_open;
	close_func real_close;
	read_func real_read;
	write_func real_write;
	ioctl_func real_ioctl;
} ProteusContext;

//=============================================================================
// Global context (defined in hook_manager.c)
//=============================================================================

extern ProteusContext g_proteusCtx;

//=============================================================================
// Utility functions
//=============================================================================

// Logging macros
#if PROTEUS_VERBOSE
#define PROTEUS_LOG(fmt, ...) \
    fprintf(stdout, "[dev-proteus] " fmt "\n", ##__VA_ARGS__)
#else
#define PROTEUS_LOG(fmt, ...) ((void)0)
#endif

#endif // PROTEUS_COMMON_H