/**
 * dev-proteus - Universal Linux Peripheral Emulator
 */

#ifndef PROTEUS_COMMON_H
#define PROTEUS_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "config.h"

 //=============================================================================
 // Base Virtual Device
 //=============================================================================

typedef enum DeviceTypeE
{
	DEV_TYPE_I2C_E = 0,
	DEV_TYPE_SPI_E,
	DEV_TYPE_UART_E,
	DEV_TYPE_GPIO_E,

	DEV_TYPE_MAX_E = 0xFF, // Keep it last
}DeviceTypeEnum;

typedef struct VirtualDeviceS
{
	DeviceTypeEnum type;
	int fd;
	char name[FILENAME_MAX];
	uint32_t id;

	int clientSock;

	int (*impl_close)(struct VirtualDeviceS* device);
	ssize_t(*impl_read)(struct VirtualDeviceS* device, void* buf, size_t len);
	ssize_t(*impl_write)(struct VirtualDeviceS* device, const void* buf, size_t count);
	int (*impl_ioctl)(struct VirtualDeviceS* device, unsigned long request, void* argp);

	struct VirtualDeviceS* next;
} VirtualDevice;

//=============================================================================
// Configuration
//=============================================================================

typedef struct
{
	char emulator_host[FILENAME_MAX];
	int emulator_port;
	int enable_i2c;
	int enable_spi;
	int enable_uart;
	int enable_gpio;
} proteus_config_t;

//=============================================================================
// Global configuration (defined in hook_manager.c)
//=============================================================================

extern proteus_config_t g_proteus_config;

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