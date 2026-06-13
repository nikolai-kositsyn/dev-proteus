/**
 * dev-proteus - Universal Linux Peripheral Emulator
 */

#ifndef PROTEUS_PROTEUS_H
#define PROTEUS_PROTEUS_H

#include <stdint.h>
#include <stddef.h>
#include <linux/limits.h> // NAME_MAX

 //=============================================================================
 // Protocol constants
 //=============================================================================

#define PROTEUS_MAGIC       		(0x50524F54)	// "PROT"
#define PROTEUS_PAYLOAD_MAX_SIZE	(8 * 1024)		// 8KB max

//=============================================================================
// Commands
//=============================================================================

typedef enum ProteusCommandE
{
	// General
	PROTEUS_CMD_GET_DEVICES = 0,

	// I2C / SMBus
	PROTEUS_CMD_I2C_SET_SLAVE = 10,
	PROTEUS_CMD_I2C_TRANSACTION,
	PROTEUS_CMD_SMBUS_TRANSACTION,

	// SPI
	PROTEUS_CMD_SPI_SET_MODE = 20,
	PROTEUS_CMD_SPI_GET_MODE,

	PROTEUS_CMD_SPI_SET_BITS_PER_WORD,
    PROTEUS_CMD_SPI_GET_BITS_PER_WORD,

    PROTEUS_CMD_SPI_SET_MAX_SPEED_HZ,
    PROTEUS_CMD_SPI_GET_MAX_SPEED_HZ,

    PROTEUS_CMD_SPI_SET_LSB_FIRST,
    PROTEUS_CMD_SPI_GET_LSB_FIRST,

    PROTEUS_CMD_SPI_SET_MODE32,
    PROTEUS_CMD_SPI_GET_MODE32,

    PROTEUS_CMD_SPI_TRANSACTION,	

	// UART
	PROTEUS_MSG_UART_READ = 40,
	PROTEUS_MSG_UART_WRITE,

	// GPIO
	PROTEUS_MSG_GPIO_READ = 50,
	PROTEUS_MSG_GPIO_WRITE,
	PROTEUS_MSG_GPIO_DIRECTION,

} ProteusCommandEnum;

//=============================================================================
// Status codes
//=============================================================================

typedef enum ProteusStatusE
{
	PROTEUS_STATUS_SUCCESS = 0,
	PROTEUS_STATUS_DEVICE_NOT_FOUND,
	PROTEUS_STATUS_WRONG_INPUT,
	PROTEUS_STATUS_COMMAND_FAILED,
	PROTEUS_STATUS_COMMAND_NOT_FOUND,

} ProteusStatus;

//=============================================================================
// Protocol structures (binary format)
//=============================================================================

#pragma pack(push, 1)

// Request Header
typedef struct
{
	uint32_t magic;
	uint16_t command;
	uint16_t sequence;	
	uint16_t payloadLen;
} ProteusReqHeader;

// Response Header
typedef struct
{
	uint32_t magic;
	uint16_t status;
	uint16_t sequence;
	uint16_t payloadLen;
} ProteusRespHeader;

//=============================================================================
// Common Defines
//=============================================================================

//=============================================================================
// Get Buses to emulate / hook
//=============================================================================

typedef struct ProteusDeviceInfoS
{
	uint8_t type;
	char name[NAME_MAX];
}ProteusDevInfo;

//=============================================================================
// Set I2C Address
//=============================================================================

//=============================================================================
// I2C Transaction
//=============================================================================

typedef struct
{
	uint16_t addr;
	uint16_t flags;
	uint16_t len;
} ProteusI2cMsgHeader;

//=============================================================================
// SMBus Transaction
//=============================================================================

// SMBus message: read_write (1B), command (1B), size (4B), block (34B)
#define PROTEUS_SMBUS_BLOCK_SIZE    (32 /* I2C_SMBUS_BLOCK_MAX */ + 2 /* length + pec */)

typedef struct
{
	uint8_t read_write;
	uint8_t command;
	uint32_t size;
	uint8_t block[PROTEUS_SMBUS_BLOCK_SIZE];
}ProteusSMBusMsg;

//=============================================================================
// SPI Transaction
//=============================================================================

typedef struct
{
	uint8_t isTx; // __u64 tx_buf;
	uint8_t isRx; // __u64 rx_buf;

	uint32_t len;
	uint32_t speed_hz;

	uint16_t delay_usecs;
	uint8_t bits_per_word;
	uint8_t cs_change;
	uint8_t tx_nbits;
	uint8_t rx_nbits;
	uint8_t word_delay_usecs;	
} ProteusSpiTransferHeader;

#pragma pack(pop)

//=============================================================================
// Socket communication
//=============================================================================

void proteus_destroy();

uint16_t proteus_next_sequence();

int proteus_connect();
void proteus_disconnect(int clientSock);

int proteus_send_request(int clientSock, uint16_t command, uint16_t sequence,
	const uint8_t* payload, uint16_t payloadLen);
int proteus_recv_response(int clientSock, uint16_t expectedSequence,
	uint8_t* payload, uint16_t payloadBufferSize, uint16_t* actualPayloadLen);

#endif // PROTEUS_PROTEUS_H