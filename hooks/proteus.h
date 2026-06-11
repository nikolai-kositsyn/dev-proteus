/**
 * dev-proteus - Universal Linux Peripheral Emulator
 */

#ifndef PROTEUS_PROTEUS_H
#define PROTEUS_PROTEUS_H

#include <stdint.h>
#include <stddef.h>

 //=============================================================================
 // Protocol constants
 //=============================================================================

#define PROTEUS_MAGIC       (0x50524F54)  // "PROT"
#define PROTEUS_MAX_PAYLOAD	(4096)

#define PROTEUS_TIMEOUT_MS	(500)
#define PROTEUS_RETRY_COUNT	(3)


//=============================================================================
// Commands
//=============================================================================

typedef enum ProteusCommandE
{
	// I2C / SMBus
	PROTEUS_CMD_I2C_SET_SLAVE = 0,
	PROTEUS_CMD_I2C_TRANSACTION = 1,
	PROTEUS_CMD_SMBUS_TRANSACTION = 2,

	// SPI
	PROTEUS_MSG_SPI_TRANSACTION = 10,

	// UART
	PROTEUS_MSG_UART_READ = 20,
	PROTEUS_MSG_UART_WRITE = 21,

	// GPIO
	PROTEUS_MSG_GPIO_READ = 30,
	PROTEUS_MSG_GPIO_WRITE = 31,
	PROTEUS_MSG_GPIO_DIRECTION = 32,
} ProteusCommandEnum;

//=============================================================================
// Status codes
//=============================================================================

typedef enum ProteusStatusE
{
	PROTEUS_STATUS_SUCCESS = 0,
	PROTEUS_STATUS_DEVICE_NOT_FOUND = 1,
	PROTEUS_STATUS_WRONG_INPUT = 2,
	PROTEUS_STATUS_COMMAND_FAILED = 3,

	PROTEUS_STATUS_TIMEOUT = 4,
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

// Defines
#define BUS_ID_FIELD_SIZE           (sizeof(uint32_t))
#define I2C_SLAVE_ADDR_FIELD_SIZE   (sizeof(uint16_t))

// I2C Message Header
typedef struct
{
	uint16_t addr;
	uint16_t flags;
	uint16_t len;
} ProteusI2cMsgHeader;

// SMBus message: read_write (1B), command (1B), size (4B), block (34B)
#define PROTEUS_SMBUS_BLOCK_SIZE    (32 /* I2C_SMBUS_BLOCK_MAX */ + 2 /* length + pec */)

typedef struct
{
	uint8_t read_write;
	uint8_t command;
	uint32_t size;
	uint8_t block[PROTEUS_SMBUS_BLOCK_SIZE];
}ProteusSMBusMsg;

// SPI Message Header
typedef struct
{
	uint8_t  cs;
	uint8_t  mode;
	uint32_t speed;
	uint8_t  bits_per_word;
	uint32_t len;
} ProteusSpiMsgHeader;

#pragma pack(pop)

//=============================================================================
// Socket communication
//=============================================================================

void proteus_init();
void proteus_destroy();

uint16_t proteus_next_sequence();

int proteus_connect();
void proteus_disconnect(int clientSock);

int proteus_send_request(int clientSock, uint16_t command, uint16_t sequence,
	const uint8_t* payload, uint16_t payloadLen);
int proteus_recv_response(int clientSock, uint16_t expectedSequence,
	uint8_t* payload, uint16_t expectedPayloadLen);

#endif // PROTEUS_PROTEUS_H