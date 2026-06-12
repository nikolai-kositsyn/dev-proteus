"""Protocol definitions for communication between hook library and emulator"""

from dataclasses import dataclass
import struct
from enum import IntEnum
from typing import Dict, List, Tuple

# Protocol constants
PROTEUS_MAGIC = 0x50524F54  # "PROT"
PROTEUS_MAX_PAYLOAD = 4096


# Commands
class ProteusCommand(IntEnum):
    # General
    GET_DEVICES = 0

    # I2C / SMBus
    I2C_SET_SLAVE = 10
    I2C_TRANSACTION = 11
    SMBUS_TRANSACTION = 12

    # SPI
    SPI_TRANSACTION = 20

    # UART
    UART_READ = 30
    UART_WRITE = 31

    # GPIO
    GPIO_READ = 40
    GPIO_WRITE = 41
    GPIO_DIRECTION = 42


# Status codes
class ProteusStatus(IntEnum):
    SUCCESS = 0
    DEVICE_NOT_FOUND = 1
    WRONG_INPUT = 2
    COMMAND_FAILED = 3
    TIMEOUT = 4


"""
Protocol structures (binary format)
"""

# Request Header: magic (4B), cmd (2B), sequence (2B), payload_len (2B)
REQ_HEADER_FORMAT = '<IHHH'
REQ_HEADER_SIZE = struct.calcsize(REQ_HEADER_FORMAT)

# Response Header: magic (4B), status (2B), sequence (2B), payload_len (2B)
RESP_HEADER_FORMAT = '<IHHH'
RESP_HEADER_SIZE = struct.calcsize(RESP_HEADER_FORMAT)

BUS_ID_FORMAT = '<I'
BUS_ID_SIZE = struct.calcsize(BUS_ID_FORMAT)

"""
I2C
"""

# Set Slave Address: address value (2B)
SET_I2C_SLAVE_CMD_FORMAT = '<H'
SET_I2C_SLAVE_CMD_SIZE = struct.calcsize(SET_I2C_SLAVE_CMD_FORMAT)

# I2C message header: addr (2B), flags (2B), len (2B)
I2C_MSG_HEADER_FORMAT = '<HHH'
I2C_MSG_HEADER_SIZE = struct.calcsize(I2C_MSG_HEADER_FORMAT)

# I2C flags (from linux/i2c.h)
I2C_M_RD = 0x0001
I2C_M_TEN = 0x0010
I2C_M_RECV_LEN = 0x0400
I2C_M_NO_RD_ACK = 0x0800
I2C_M_IGNORE_NAK = 0x1000
I2C_M_REV_DIR_ADDR = 0x2000
I2C_M_NOSTART = 0x4000
I2C_M_STOP = 0x8000

"""
SMBus
"""

# SMBus message: read_write (1B), command (1B), size (4B), block (34B)
SMBUS_MSG_FORMAT = '<BBI34s'
SMBUS_MSG_SIZE = struct.calcsize(SMBUS_MSG_FORMAT)

I2C_SMBUS_READ = 1
I2C_SMBUS_WRITE = 0

I2C_SMBUS_QUICK = 0
I2C_SMBUS_BYTE = 1
I2C_SMBUS_BYTE_DATA = 2
I2C_SMBUS_WORD_DATA = 3
I2C_SMBUS_PROC_CALL = 4
I2C_SMBUS_BLOCK_DATA = 5
I2C_SMBUS_I2C_BLOCK_DATA = 8

# SMBus block
SMBUS_BLOCK_MAX = 32
SMBUS_BLOCK_SIZE = SMBUS_BLOCK_MAX + 2


@dataclass
class SMBusData:
    read_write: int
    command: int
    size: int
    block: bytes


"""
SPI
"""

# SPI message header: cs (1B), mode (1B), speed (4B), bits (1B), len (4B)
SPI_MSG_FORMAT = '<BBIBI'
SPI_MSG_SIZE = struct.calcsize(SPI_MSG_FORMAT)


class ProtocolMessage:
    """Protocol message builder/parser"""

    @classmethod
    def decode_set_i2c_slave(cls, req_payload: bytes) -> Tuple[int, int]:
        """Decode Set I2C Slave Address transaction payload"""

        if len(req_payload) < BUS_ID_SIZE + SET_I2C_SLAVE_CMD_SIZE:
            raise ValueError("Invalid 'Set I2C Slave Address' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        slave_address, = struct.unpack(SET_I2C_SLAVE_CMD_FORMAT,
                                       req_payload[BUS_ID_SIZE:BUS_ID_SIZE + SET_I2C_SLAVE_CMD_SIZE])

        return bus_id, slave_address

    @classmethod
    def decode_i2c_transaction(cls, req_payload: bytes) -> Tuple[int, List]:
        """Decode I2C transaction payload into bus id and messages"""

        MSG_COUNT_SIZE = 4

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        msg_count, = struct.unpack('<I', req_payload[BUS_ID_SIZE: BUS_ID_SIZE + MSG_COUNT_SIZE])

        messages = []
        offset = BUS_ID_SIZE + MSG_COUNT_SIZE

        for _ in range(msg_count):
            if offset + I2C_MSG_HEADER_SIZE > len(req_payload):
                raise ValueError("Truncated I2C message")

            addr, flags, length = struct.unpack(I2C_MSG_HEADER_FORMAT, req_payload[offset:offset + I2C_MSG_HEADER_SIZE])
            offset += I2C_MSG_HEADER_SIZE

            data = b''
            if not (flags & I2C_M_RD) and length > 0:
                if offset + length > len(req_payload):
                    raise ValueError("Truncated I2C write data")
                data = req_payload[offset:offset + length]
                offset += length

            messages.append({
                'addr': addr,
                'flags': flags,
                'length': length,
                'data': data,
                'is_read': bool(flags & I2C_M_RD)
            })

        return bus_id, messages

    @classmethod
    def decode_smbus_transaction(cls, payload: bytes) -> Tuple[int, SMBusData]:
        """Decode SMBus transaction payload into bus id and data"""

        if len(payload) < BUS_ID_SIZE + SMBUS_MSG_SIZE:
            raise ValueError("Invalid SMBus transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, payload[:BUS_ID_SIZE])

        read_write, command, size, block = struct.unpack(SMBUS_MSG_FORMAT,
                                                         payload[BUS_ID_SIZE:BUS_ID_SIZE + SMBUS_MSG_SIZE])

        return bus_id, SMBusData(read_write=read_write, command=command, size=size, block=block)

    @classmethod
    def encode_smbus_transaction(cls, data: SMBusData) -> bytes:
        """Encode SMBus transaction"""
        payload = struct.pack(SMBUS_MSG_FORMAT, data.read_write, data.command, data.size, data.block)
        return payload
