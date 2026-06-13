/**
 * dev-proteus - I2C Functionality
 * Handles I2C device operations (/dev/i2c-*)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "common.h"
#include "proteus.h"

 //=============================================================================
 // I2C private types
 //=============================================================================

typedef struct I2cDeviceS
{
	/* Common instance already includes I2C bus ID field */
	VirtualDevice base;

	/*
		Keep 10 bits addressing flag set by 'I2C_TENBIT'.
	*/
	uint8_t enable10bitsAddress;

	/*
		Keep slave address set by 'I2C_SLAVE' and 'I2C_SLAVE_FORCE'.
		It is used in 'i2c_hook_read' and 'i2c_hook_write' functions
	*/
	uint16_t slaveAddress;

}I2cDevice;

//=============================================================================
// I2C server communication functions
//=============================================================================

static int set_i2c_slave(I2cDevice* bus, uint16_t slaveAddress)
{
	// Build payload
	uint8_t payload[sizeof(bus->base.id) + sizeof(slaveAddress)];
	uint16_t offset = 0;

	// Bus Id
	memcpy(payload + offset, &bus->base.id, sizeof(bus->base.id));
	offset += sizeof(bus->base.id);

	// Slave Address
	memcpy(payload + offset, &slaveAddress, sizeof(slaveAddress));
	offset += sizeof(slaveAddress);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();

	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_I2C_SET_SLAVE, 
		sequence, payload, offset) < 0)
	{
		return -1;
	}
	
	return proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
}

static void apply_i2c_read_data(struct i2c_rdwr_ioctl_data* i2cData, 
	const uint8_t* readData, uint16_t readDataLen)
{
	uint16_t offset = 0;
	for (uint32_t i = 0; i < i2cData->nmsgs; ++i)
	{
		if (i2cData->msgs[i].flags & I2C_M_RD)
		{
			uint16_t bytesToCopy = i2cData->msgs[i].len;
			if (offset + bytesToCopy > readDataLen)
			{
				bytesToCopy = readDataLen - offset;
			}

			memcpy(i2cData->msgs[i].buf, readData + offset, bytesToCopy);
			offset += bytesToCopy;
		}
	}
}

static int run_i2c_transaction(I2cDevice* bus, struct i2c_rdwr_ioctl_data* data)
{
	// Calculate request payload size
	uint16_t reqPayloadLen = sizeof(bus->base.id); // Bus id
	reqPayloadLen += sizeof(data->nmsgs); // Messages count

	for (uint32_t i = 0; i < data->nmsgs; ++i)
	{
		reqPayloadLen += sizeof(ProteusI2cMsgHeader);
		if (!(data->msgs[i].flags & I2C_M_RD))
		{
			reqPayloadLen += data->msgs[i].len;
		}
	}

	if (reqPayloadLen > PROTEUS_PAYLOAD_MAX_SIZE)
	{
		PROTEUS_LOG("Request payload too large: %u > %u", reqPayloadLen, PROTEUS_PAYLOAD_MAX_SIZE);

		errno = EFBIG;
		return -1;
	}

	// Build request payload
	uint8_t payload[PROTEUS_PAYLOAD_MAX_SIZE];
	uint16_t offset = 0;

	// Bus id
	memcpy(payload + offset, &bus->base.id, sizeof(bus->base.id));
	offset += sizeof(bus->base.id);

	// Messages count
	memcpy(payload + offset, &data->nmsgs, sizeof(data->nmsgs));
	offset += sizeof(data->nmsgs);

	// Messages
	for (uint32_t i = 0; i < data->nmsgs; ++i)
	{
		ProteusI2cMsgHeader msg;
		msg.addr = data->msgs[i].addr;
		msg.flags = data->msgs[i].flags;
		msg.len = data->msgs[i].len;

		memcpy(payload + offset, &msg, sizeof(msg));
		offset += sizeof(msg);

		if (!(msg.flags & I2C_M_RD) && msg.len > 0)
		{
			memcpy(payload + offset, data->msgs[i].buf, msg.len);
			offset += msg.len;
		}
	}

	// Run transaction
	uint16_t sequence = proteus_next_sequence();

	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_I2C_TRANSACTION,
		sequence, payload, offset) < 0)
	{
		return -1;
	}
	
	uint16_t respPayloadLen = 0;	
	if (proteus_recv_response(bus->base.clientSock, sequence, payload, sizeof(payload), &respPayloadLen) < 0)
	{
		return -1;
	}

	if (respPayloadLen > 0)
	{
		apply_i2c_read_data(data, payload, respPayloadLen);
	}

	return 0;
}

static int run_smbus_transaction(I2cDevice* bus, struct i2c_smbus_ioctl_data* smbusData)
{
	// Build payload
	uint8_t payload[sizeof(bus->base.id) + sizeof(ProteusSMBusMsg)];
	uint16_t offset = 0;
	int result = 0;

	// Bus id
	memcpy(payload + offset, &bus->base.id, sizeof(bus->base.id));
	offset += sizeof(bus->base.id);

	// SMBus message
	ProteusSMBusMsg* msg = (ProteusSMBusMsg*)&payload[offset];
	msg->read_write = smbusData->read_write;
	msg->command = smbusData->command;
	msg->size = smbusData->size;
	memset(msg->block, 0, sizeof(msg->block));

	// Serialize request data
	if (smbusData->data && smbusData->read_write == I2C_SMBUS_WRITE)
	{
		switch (smbusData->size)
		{
		case I2C_SMBUS_BYTE:
		case I2C_SMBUS_BYTE_DATA:
			msg->block[0] = 1;
			msg->block[1] = smbusData->data->byte;
			break;

		case I2C_SMBUS_WORD_DATA:
		case I2C_SMBUS_PROC_CALL:
			msg->block[0] = 2;
			msg->block[1] = (smbusData->data->word >> 8) & 0xFF;
			msg->block[2] = smbusData->data->word & 0xFF;
			break;

		case I2C_SMBUS_BLOCK_DATA:
		case I2C_SMBUS_I2C_BLOCK_DATA:
			memcpy(msg->block, smbusData->data->block, sizeof(msg->block));
			break;

		default:
			// QUICK, etc. - no data
			break;
		}
	}

	offset += sizeof(ProteusSMBusMsg);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();

	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SMBUS_TRANSACTION,
		sequence, payload, offset) < 0)
	{
		return -1;
	}

	// Handle response based on operation type
	// Special case: PROCESS_CALL is WRITE but expects response
	uint8_t expectsResponse = (smbusData->read_write == I2C_SMBUS_READ) ||
		(smbusData->size == I2C_SMBUS_PROC_CALL);
	if (expectsResponse)
	{
		// READ operation: response with data expected
		uint16_t respPayloadLen = 0;
		if (proteus_recv_response(bus->base.clientSock, sequence, (uint8_t*)msg, sizeof(ProteusSMBusMsg), &respPayloadLen) < 0)
		{
			return -1;
		}

		// Deserialize response data
		if (smbusData->data)
		{
			uint8_t dataLen = msg->block[0];

			switch (smbusData->size)
			{
			case I2C_SMBUS_BYTE:
			case I2C_SMBUS_BYTE_DATA:
				if (dataLen >= 1) {
					smbusData->data->byte = msg->block[1];
				}
				break;

			case I2C_SMBUS_WORD_DATA:
			case I2C_SMBUS_PROC_CALL:
				if (dataLen >= 2) {
					smbusData->data->word = (msg->block[1] << 8) | msg->block[2];
				}
				break;

			case I2C_SMBUS_BLOCK_DATA:
			case I2C_SMBUS_I2C_BLOCK_DATA:
				memcpy(smbusData->data->block, msg->block, sizeof(msg->block));
				break;

			default:
				// No data to copy
				break;
			}
		}
	}
	else
	{
		// WRITE operation: payload not expected
		result = proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
	}

	return result;
}

//=============================================================================
// Forward declarations for I2C implementations
//=============================================================================

static int i2c_hook_close(VirtualDevice* device);
static ssize_t i2c_hook_read(VirtualDevice* device, void* buf, size_t len);
static ssize_t i2c_hook_write(VirtualDevice* device, const void* buf, size_t count);
static int i2c_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

//=============================================================================
// I2C 'open' implementation
//=============================================================================

void* i2c_hook_open(const char* name, int flags, ...)
{
	(void)flags;

	PROTEUS_LOG("[I2C] %s: open, flags=%X", name, flags);

	I2cDevice* device = NULL;

	// Try to extract bus number
	int busId = -1;
	if (sscanf(name, "/dev/i2c-%d", &busId) != 1 || busId < 0)
	{
		PROTEUS_LOG("Failed to extract I2C bus number from %s", name);
		return device;
	}

	int fd = g_proteusCtx.real_open("/dev/null", O_RDWR);
	if (fd > 0)
	{
		device = (I2cDevice*)malloc(sizeof(I2cDevice));
		if (device != NULL)
		{
			/* Connect to emulator */
			int clientSock = proteus_connect();
			if (clientSock >= 0)
			{
				/* Common */
				device->base.fd = fd;
				device->base.type = DEV_TYPE_I2C_E;

				(void)strncpy(device->base.name, name, sizeof(device->base.name) - 1);

				device->base.id = (uint32_t)busId;
				device->base.clientSock = clientSock;				

				device->base.impl_close = i2c_hook_close;
				device->base.impl_read = i2c_hook_read;
				device->base.impl_write = i2c_hook_write;
				device->base.impl_ioctl = i2c_hook_ioctl;

				device->base.next = NULL;

				/* I2C Specific */
				device->enable10bitsAddress = 0x00;
				device->slaveAddress = 0x00;
			}
			else
			{
				g_proteusCtx.real_close(fd);

				free((void*)device);
				device = NULL;
			}
		}
		else
		{
			PROTEUS_LOG("Failed to malloc when create I2C device");
			g_proteusCtx.real_close(fd);
		}
	}
	else
	{
		PROTEUS_LOG("Failed to open '/dev/null' when create I2C device (%d)", fd);
	}

	return (void*)device;
}

//=============================================================================
// I2C 'close', 'read', 'write' and 'ioctl' implementation
//=============================================================================

static int i2c_hook_close(VirtualDevice* device)
{
	PROTEUS_LOG("[I2C] %s: close, fd=%d", device->name, device->fd);	

	int closeResult = g_proteusCtx.real_close(device->fd);
	proteus_disconnect(device->clientSock);
	free((void*)device);

	return closeResult;
}

static ssize_t i2c_hook_read(VirtualDevice* device, void* buf, size_t len)
{
	PROTEUS_LOG("[I2C] %s: read, fd=%d", device->name, device->fd);

	I2cDevice* bus = (I2cDevice*)device;

	// Prepare and handle 'read' message only
	struct i2c_msg msg = { .addr = bus->slaveAddress, .flags = I2C_M_RD, .len = len, .buf = buf };
	struct i2c_rdwr_ioctl_data data = { .msgs = &msg, .nmsgs = 1 };

	int result = run_i2c_transaction(bus, &data);
	if (result == 0)
	{
		return (ssize_t)data.msgs[0].len;
	}

	return result;
}

static ssize_t i2c_hook_write(VirtualDevice* device, const void* buf, size_t count)
{
	PROTEUS_LOG("[I2C] %s: write, fd=%d", device->name, device->fd);

	I2cDevice* bus = (I2cDevice*)device;

	// Prepare and send 'write' message only
	struct i2c_msg msg = { .addr = bus->slaveAddress, .flags = 0, .len = count, .buf = (void*)buf };
	struct i2c_rdwr_ioctl_data data = { .msgs = &msg, .nmsgs = 1 };

	int result = run_i2c_transaction(bus, &data);
	if (result == 0)
	{
		return (ssize_t)data.msgs[0].len;
	}

	return result;
}

static int i2c_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp)
{
	I2cDevice* bus = (I2cDevice*)device;

	int ioctlResult = 0;

	switch (request)
	{
	case I2C_SLAVE: /* Set address */
	case I2C_SLAVE_FORCE: /* Set address without checking */
	{
		uint16_t slaveAddress = (uint16_t)((unsigned long)argp);
		PROTEUS_LOG("[I2C] %s: I2C_SLAVE | I2C_SLAVE_FORCE for 0x%04X", device->name, slaveAddress);

		ioctlResult = set_i2c_slave(bus, slaveAddress);
		if (ioctlResult == 0)
		{
			bus->slaveAddress = slaveAddress;
		}

		break;
	}

	case I2C_TENBIT: /* Enable/Disable 10-bit addressing */
	{
		bus->enable10bitsAddress = (uint8_t)((unsigned long)argp);
		PROTEUS_LOG("[I2C] %s: I2C_TENBIT %s", device->name,
			bus->enable10bitsAddress ? "enabled" : "disabled");

		break;
	}

	case I2C_FUNCS: /* Send supported functions */
	{
		/*
			I2C_FUNC_I2C - Plain i2c-level commands
			I2C_FUNC_SMBUS_EMUL - Handles all SMBus commands that can be
			emulated by a real I2C adapter (using the transparent emulation layer)
		*/

		unsigned long* funcs = (unsigned long*)argp;
		*funcs = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
		PROTEUS_LOG("[I2C] %s: get functions (0x%08lX)", device->name, *funcs);

		break;
	}

	case I2C_RDWR:
	{
		struct i2c_rdwr_ioctl_data* i2cData = (struct i2c_rdwr_ioctl_data*)argp;

		PROTEUS_LOG("[I2C] %s: I2C_RDWR with %u messages, fd=%d", device->name, i2cData->nmsgs, device->fd);

		ioctlResult = run_i2c_transaction(bus, i2cData);

		break;
	}

	case I2C_SMBUS:
	{
		struct i2c_smbus_ioctl_data* smbusData = (struct i2c_smbus_ioctl_data*)argp;

		PROTEUS_LOG("[I2C] %s: I2C_SMBUS (read_write=%d, command=0x%02X, size=%d)",
			device->name, smbusData->read_write, smbusData->command, smbusData->size);

		ioctlResult = run_smbus_transaction(bus, smbusData);

		break;
	}

	default:
	{
		PROTEUS_LOG("[I2C] %s: Unsupported ioctl request: 0x%08lX", device->name, request);

		ioctlResult = -1;
		errno = ENOTTY;

		break;
	}
	}

	return ioctlResult;
}
