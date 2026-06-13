/**
 * dev-proteus - SPI Functionality
 * Handles SPI device operations (/dev/spidev*.*)
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
#include <linux/spi/spidev.h>

#include "common.h"
#include "proteus.h"

 //=============================================================================
 // SPI private types
 //=============================================================================

typedef struct SpiDeviceS
{
	/* Common instance already includes SPI bus ID field */
	VirtualDevice base;	

	/*
		Keep CS address.
		It is used in 'spi_hook_read' and 'spi_hook_write' functions
	*/
	uint8_t chipSelect;

}SpiDevice;

//=============================================================================
// SPI server communication functions
//=============================================================================

static int set_spi_mode(SpiDevice* bus, uint8_t mode)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id) + sizeof(mode)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// Mode
	memcpy(reqPayload + reqPayloadLen, &mode, sizeof(mode));
	reqPayloadLen += sizeof(mode);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_SET_MODE, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}
	
	return proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
}

static int get_spi_mode(SpiDevice* bus, uint8_t* mode)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_GET_MODE, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}	
    
	uint16_t respPayloadLen = 0;
	if(proteus_recv_response(bus->base.clientSock, sequence, mode, sizeof(*mode), &respPayloadLen) < 0)
    {
        return -1;
    }

    if (respPayloadLen != sizeof(*mode))
    {
        return -1;
    }

    return 0;
}

static int set_spi_bits_per_word(SpiDevice* bus, uint8_t bits_per_word)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id) + sizeof(bits_per_word)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// bits_per_word
	memcpy(reqPayload + reqPayloadLen, &bits_per_word, sizeof(bits_per_word));
	reqPayloadLen += sizeof(bits_per_word);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_SET_BITS_PER_WORD, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}
	
	return proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
}

static int get_spi_bits_per_word(SpiDevice* bus, uint8_t* bits_per_word)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_GET_BITS_PER_WORD, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}	
    
	uint16_t respPayloadLen = 0;
	if(proteus_recv_response(bus->base.clientSock, sequence, bits_per_word, sizeof(*bits_per_word), &respPayloadLen) < 0)
    {
        return -1;
    }

    if (respPayloadLen != sizeof(*bits_per_word))
    {
        return -1;
    }

    return 0;
}

static int set_spi_max_speed_hz(SpiDevice* bus, uint32_t max_speed_hz)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id) + sizeof(max_speed_hz)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// max_speed_hz
	memcpy(reqPayload + reqPayloadLen, &max_speed_hz, sizeof(max_speed_hz));
	reqPayloadLen += sizeof(max_speed_hz);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_SET_MAX_SPEED_HZ, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}
	
	return proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
}

static int get_spi_max_speed_hz(SpiDevice* bus, uint32_t* max_speed_hz)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_GET_MAX_SPEED_HZ, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}	
    
	uint16_t respPayloadLen = 0;
	if(proteus_recv_response(bus->base.clientSock, sequence, 
        (uint8_t*)max_speed_hz, sizeof(*max_speed_hz), &respPayloadLen) < 0)
    {
        return -1;
    }

    if (respPayloadLen != sizeof(*max_speed_hz))
    {
        return -1;
    }

    return 0;
}

static int set_spi_lsb_first(SpiDevice* bus, uint8_t lsb_first)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id) + sizeof(lsb_first)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// lsb_first
	memcpy(reqPayload + reqPayloadLen, &lsb_first, sizeof(lsb_first));
	reqPayloadLen += sizeof(lsb_first);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_SET_LSB_FIRST, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}
	
	return proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
}

static int get_spi_lsb_first(SpiDevice* bus, uint8_t* lsb_first)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_GET_LSB_FIRST, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}	
    
	uint16_t respPayloadLen = 0;
	if(proteus_recv_response(bus->base.clientSock, sequence, lsb_first, sizeof(*lsb_first), &respPayloadLen) < 0)
    {
        return -1;
    }

    if (respPayloadLen != sizeof(*lsb_first))
    {
        return -1;
    }

    return 0;
}

static int set_spi_mode32(SpiDevice* bus, uint32_t mode32)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id) + sizeof(mode32)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// mode32
	memcpy(reqPayload + reqPayloadLen, &mode32, sizeof(mode32));
	reqPayloadLen += sizeof(mode32);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_SET_MODE32, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}
	
	return proteus_recv_response(bus->base.clientSock, sequence, NULL, 0, NULL);
}

static int get_spi_mode32(SpiDevice* bus, uint32_t* mode32)
{
	// Build payload
	uint8_t reqPayload[sizeof(bus->base.id)];
	uint16_t reqPayloadLen = 0;

	// Bus Id
	memcpy(reqPayload + reqPayloadLen, &bus->base.id, sizeof(bus->base.id));
	reqPayloadLen += sizeof(bus->base.id);

	// Run transaction
	uint16_t sequence = proteus_next_sequence();
	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_GET_MODE32, 
		sequence, reqPayload, reqPayloadLen) < 0)
	{
		return -1;
	}	
    
	uint16_t respPayloadLen = 0;
	if(proteus_recv_response(bus->base.clientSock, sequence, 
        (uint8_t*)mode32, sizeof(*mode32), &respPayloadLen) < 0)
    {
        return -1;
    }

    if (respPayloadLen != sizeof(*mode32))
    {
        return -1;
    }

    return 0;
}

static int run_spi_transaction(SpiDevice* bus, struct spi_ioc_transfer* transfers, uint32_t nmsgs)
{
	// Calculate request payload size
	uint16_t reqPayloadLen = sizeof(bus->base.id); // Bus id
    reqPayloadLen += sizeof(bus->chipSelect); // CS id
	reqPayloadLen += sizeof(nmsgs); // Messages count

	for (uint32_t i = 0; i < nmsgs; ++i)
	{
        struct spi_ioc_transfer* transfer = &transfers[i];

		reqPayloadLen += sizeof(ProteusSpiTransferHeader);
		if (transfer->tx_buf)
		{
			reqPayloadLen += transfer->len;
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

    // CS id
	memcpy(payload + offset, &bus->chipSelect, sizeof(bus->chipSelect));
	offset += sizeof(bus->chipSelect);

	// Messages count
	memcpy(payload + offset, &nmsgs, sizeof(nmsgs));
	offset += sizeof(nmsgs);

	// Messages
	for (uint32_t i = 0; i < nmsgs; ++i)
	{
        struct spi_ioc_transfer* transfer = &transfers[i];

		ProteusSpiTransferHeader header;        
        header.isTx = transfer->tx_buf != 0;
        header.isRx = transfer->rx_buf != 0;
		header.len = transfer->len;
        header.speed_hz = transfer->speed_hz;
        header.delay_usecs = transfer->delay_usecs;
	    header.bits_per_word = transfer->bits_per_word;
	    header.cs_change = transfer->cs_change;
	    header.tx_nbits = transfer->tx_nbits;
	    header.rx_nbits = transfer->rx_nbits;
	    header.word_delay_usecs = transfer->word_delay_usecs;

		memcpy(payload + offset, &header, sizeof(header));
		offset += sizeof(header);

		if (header.isTx)
		{
			memcpy(payload + offset, (void*)transfer->tx_buf, header.len);
			offset += header.len;
		}
	}

	// Run transaction
	uint16_t sequence = proteus_next_sequence();

	if (proteus_send_request(bus->base.clientSock, PROTEUS_CMD_SPI_TRANSACTION,
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
        // Apply received data to transfers

        uint16_t respOffset = 0;
        for (uint32_t i = 0; i < nmsgs; ++i)
        {
            struct spi_ioc_transfer* transfer = &transfers[i];

            if (transfer->rx_buf)
            {
                uint16_t bytesToCopy = transfer->len;
                if (respOffset + bytesToCopy > respPayloadLen)
                {
                    bytesToCopy = respPayloadLen - respOffset;
                }

                memcpy((void*)transfer->rx_buf, payload + respOffset, bytesToCopy);
                respOffset += bytesToCopy;
            }
        }
	}

	return 0;
}

//=============================================================================
// Forward declarations for SPI implementations
//=============================================================================

static int spi_hook_close(VirtualDevice* device);
static ssize_t spi_hook_read(VirtualDevice* device, void* buf, size_t len);
static ssize_t spi_hook_write(VirtualDevice* device, const void* buf, size_t count);
static int spi_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

//=============================================================================
// SPI 'open' implementation
//=============================================================================

void* spi_hook_open(const char* name, int flags, ...)
{
	(void)flags;

	PROTEUS_LOG("[SPI] %s: open, flags=%X", name, flags);

	SpiDevice* device = NULL;

	// Try to extract bus and cs numbers
	int busId = -1;
    int csId = -1;
	if (sscanf(name, "/dev/spidev%d.%d", &busId, &csId) != 2 || busId < 0 || csId < 0)
	{
		PROTEUS_LOG("Failed to extract SPI bus or cs number from %s", name);
		return device;
	}

	int fd = g_proteusCtx.real_open("/dev/null", O_RDWR);
	if (fd > 0)
	{
		device = (SpiDevice*)malloc(sizeof(SpiDevice));
		if (device != NULL)
		{
			/* Connect to emulator */
			int clientSock = proteus_connect();
			if (clientSock >= 0)
			{
				/* Common */
				device->base.fd = fd;
				device->base.type = DEV_TYPE_SPI_E;

				(void)strncpy(device->base.name, name, sizeof(device->base.name) - 1);

				device->base.id = (uint32_t)busId;
				device->base.clientSock = clientSock;				

				device->base.impl_close = spi_hook_close;
				device->base.impl_read = spi_hook_read;
				device->base.impl_write = spi_hook_write;
				device->base.impl_ioctl = spi_hook_ioctl;

				device->base.next = NULL;

				/* SPI Specific */
				device->chipSelect = (uint8_t)csId;
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
			PROTEUS_LOG("Failed to malloc when create SPI device");
			g_proteusCtx.real_close(fd);
		}
	}
	else
	{
		PROTEUS_LOG("Failed to open '/dev/null' when create SPI device (%d)", fd);
	}

	return (void*)device;
}

//=============================================================================
// SPI 'close', 'read', 'write' and 'ioctl' implementation
//=============================================================================

static int spi_hook_close(VirtualDevice* device)
{
	PROTEUS_LOG("[SPI] %s: close, fd=%d", device->name, device->fd);

	int closeResult = g_proteusCtx.real_close(device->fd);
	proteus_disconnect(device->clientSock);
	free((void*)device);

	return closeResult;
}

static ssize_t spi_hook_read(VirtualDevice* device, void* buf, size_t len)
{
	PROTEUS_LOG("[SPI] %s: read, fd=%d", device->name, device->fd);

	SpiDevice* bus = (SpiDevice*)device;

	// Handle 'read' transfer only
	struct spi_ioc_transfer transfer = { .tx_buf = 0, .rx_buf = (uint64_t)buf, .len = len };

	int result = run_spi_transaction(bus, &transfer, 1);
	if (result == 0)
	{
		return (ssize_t)transfer.len;
	}

	return result;
}

static ssize_t spi_hook_write(VirtualDevice* device, const void* buf, size_t count)
{
	PROTEUS_LOG("[SPI] %s: write, fd=%d", device->name, device->fd);

	SpiDevice* bus = (SpiDevice*)device;

	// Handle 'write' transfer only
	struct spi_ioc_transfer transfer = { .tx_buf = (uint64_t)buf, .rx_buf = 0, .len = count };

	int result = run_spi_transaction(bus, &transfer, 1);
	if (result == 0)
	{
		return (ssize_t)transfer.len;
	}

	return result;
}

static int spi_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp)
{
	SpiDevice* bus = (SpiDevice*)device;

	int ioctlResult = 0;

	switch (request)
	{
    // mode
	case SPI_IOC_WR_MODE:
	{
		uint8_t mode = *((uint8_t*)argp);
		PROTEUS_LOG("[SPI] %s: SPI_IOC_WR_MODE %u", device->name, mode);

		ioctlResult = set_spi_mode(bus, mode);
		break;
	}

    case SPI_IOC_RD_MODE:
	{		
		PROTEUS_LOG("[SPI] %s: SPI_IOC_RD_MODE", device->name);

		ioctlResult = get_spi_mode(bus, (uint8_t*)argp);
		break;
	}

    // bits per word
	case SPI_IOC_WR_BITS_PER_WORD:
	{
		uint8_t bitsPerWord = *((uint8_t*)argp);
		PROTEUS_LOG("[SPI] %s: SPI_IOC_WR_BITS_PER_WORD %u", device->name, bitsPerWord);

		ioctlResult = set_spi_bits_per_word(bus, bitsPerWord);
		break;
	}

    case SPI_IOC_RD_BITS_PER_WORD:
	{		
		PROTEUS_LOG("[SPI] %s: SPI_IOC_RD_BITS_PER_WORD", device->name);

		ioctlResult = get_spi_bits_per_word(bus, (uint8_t*)argp);
		break;
	}

    // max speed hz
	case SPI_IOC_WR_MAX_SPEED_HZ:
	{
		uint32_t maxSpeedHz = *((uint32_t*)argp);
		PROTEUS_LOG("[SPI] %s: SPI_IOC_WR_MAX_SPEED_HZ %u", device->name, maxSpeedHz);

		ioctlResult = set_spi_max_speed_hz(bus, maxSpeedHz);
		break;
	}

    case SPI_IOC_RD_MAX_SPEED_HZ:
	{		
		PROTEUS_LOG("[SPI] %s: SPI_IOC_RD_MAX_SPEED_HZ", device->name);

		ioctlResult = get_spi_max_speed_hz(bus, (uint32_t*)argp);
		break;
	}

    // lsb first
	case SPI_IOC_WR_LSB_FIRST:
	{
		uint8_t lsbFirst = *((uint8_t*)argp);
		PROTEUS_LOG("[SPI] %s: SPI_IOC_WR_LSB_FIRST %u", device->name, lsbFirst);

		ioctlResult = set_spi_lsb_first(bus, lsbFirst);
		break;
	}

    case SPI_IOC_RD_LSB_FIRST:
	{		
		PROTEUS_LOG("[SPI] %s: SPI_IOC_RD_LSB_FIRST", device->name);

		ioctlResult = get_spi_lsb_first(bus, (uint8_t*)argp);
		break;
	}

    // mode32
	case SPI_IOC_WR_MODE32:
	{
		uint32_t mode32 = *((uint32_t*)argp);
		PROTEUS_LOG("[SPI] %s: SPI_IOC_WR_MODE32 %u", device->name, mode32);

		ioctlResult = set_spi_mode32(bus, mode32);
		break;
	}

    case SPI_IOC_RD_MODE32:
	{		
		PROTEUS_LOG("[SPI] %s: SPI_IOC_RD_MODE32", device->name);

		ioctlResult = get_spi_mode32(bus, (uint32_t*)argp);
		break;
	}

	default:
	{
        unsigned long SPI_IOC_MSG_REF = SPI_IOC_MESSAGE(1);
        if (_IOC_TYPE(request) == _IOC_TYPE(SPI_IOC_MSG_REF) && 
            _IOC_NR(request) == _IOC_NR(SPI_IOC_MSG_REF))
        {
            uint32_t nmsgs = _IOC_SIZE(request) / sizeof(struct spi_ioc_transfer);
            PROTEUS_LOG("[SPI] %s: SPI_IOC_MESSAGE(%u), fd=%d", device->name, nmsgs, device->fd);

            ioctlResult = run_spi_transaction(bus, (struct spi_ioc_transfer*)argp, nmsgs);
        }
        else
        {
            PROTEUS_LOG("[SPI] %s: Unsupported ioctl request: 0x%08lX", device->name, request);

		    ioctlResult = -1;
		    errno = ENOTTY;
        }		

		break;
	}
	}

	return ioctlResult;
}
