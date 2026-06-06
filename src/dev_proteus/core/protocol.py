"""Protocol definitions for communication between hook library and emulator"""

from dataclasses import dataclass
import struct
from enum import IntEnum
from typing import Dict, List, Tuple

# Protocol constants
PROTEUS_MAGIC = 0x50524F54  # "PROT" in hex
DEFAULT_PORT = 4242
MAX_BUFFER_SIZE = 4096


# Message types
class ProteusMessageType(IntEnum):
    # I2C / SMBus
    I2C_SET_SLAVE = 0
    I2C_TRANSACTION = 1
    SMBUS_TRANSACTION = 2

    # SPI
    SPI_TRANSACTION = 10

    # UART
    UART_READ = 20
    UART_WRITE = 21

    # GPIO
    GPIO_READ = 30
    GPIO_WRITE = 31
    GPIO_DIRECTION = 32


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

# Header format: magic (4B), msg_type (4B), sequence (4B), payload_len (4B)
HEADER_FORMAT = '<IIII'
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# Footer format: status (4B), reserved (4B)
FOOTER_FORMAT = '<II'
FOOTER_SIZE = struct.calcsize(FOOTER_FORMAT)

BUS_ID_FORMAT = '<I'
BUS_ID_SIZE = struct.calcsize(BUS_ID_FORMAT)

"""
I2C
"""

# Set Slave Address: address value (2B)
SET_I2C_SLAVE_MSG_FORMAT = '<H'
SET_I2C_SLAVE_MSG_SIZE = struct.calcsize(SET_I2C_SLAVE_MSG_FORMAT)

# I2C message header: addr (2B), flags (2B), len (4B)
I2C_MSG_FORMAT = '<HHI'
I2C_MSG_SIZE = struct.calcsize(I2C_MSG_FORMAT)

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

# I2C flags (from linux/i2c.h)
I2C_M_RD = 0x0001
I2C_M_TEN = 0x0010
I2C_M_RECV_LEN = 0x0400
I2C_M_NO_RD_ACK = 0x0800
I2C_M_IGNORE_NAK = 0x1000
I2C_M_REV_DIR_ADDR = 0x2000
I2C_M_NOSTART = 0x4000
I2C_M_STOP = 0x8000


class ProtocolMessage:
    """Protocol message builder/parser"""

    def __init__(self):
        self.magic = PROTEUS_MAGIC
        self.msg_type = 0
        self.sequence = 0
        self.payload = b''

    @classmethod
    def encode(cls, msg_type: int, sequence: int, payload: bytes) -> bytes:
        """Encode message to bytes"""
        header = struct.pack(HEADER_FORMAT, PROTEUS_MAGIC, msg_type, sequence, len(payload))
        return header + payload

    @classmethod
    def decode(cls, data: bytes) -> tuple:
        """Decode message from bytes, returns (msg_type, sequence, payload)"""
        if len(data) < HEADER_SIZE:
            raise ValueError(f"Message too short: {len(data)} < {HEADER_SIZE}")

        magic, msg_type, sequence, payload_len = struct.unpack(HEADER_FORMAT, data[:HEADER_SIZE])

        if magic != PROTEUS_MAGIC:
            raise ValueError(f"Invalid magic: 0x{magic:08X}")

        if len(data) < HEADER_SIZE + payload_len:
            raise ValueError(f"Incomplete payload: expected {payload_len}, got {len(data) - HEADER_SIZE}")

        payload = data[HEADER_SIZE:HEADER_SIZE + payload_len]

        return msg_type, sequence, payload

    @classmethod
    def decode_set_i2c_slave(cls, payload: bytes) -> Tuple[int, int]:
        """Decode Set I2C Slave Address transaction payload"""

        if len(payload) < BUS_ID_SIZE + SET_I2C_SLAVE_MSG_SIZE:
            raise ValueError("Invalid 'Set I2C Slave Address' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, payload[:BUS_ID_SIZE])
        slave_address, = struct.unpack(SET_I2C_SLAVE_MSG_FORMAT,
                                       payload[BUS_ID_SIZE:BUS_ID_SIZE + SET_I2C_SLAVE_MSG_SIZE])

        return bus_id, slave_address

    @classmethod
    def encode_i2c_transaction(cls, sequence: int, messages: List) -> bytes:
        """Encode I2C transaction with multiple messages"""
        payload = struct.pack('<I', len(messages))  # Number of messages

        for msg in messages:
            # Message header
            payload += struct.pack(I2C_MSG_FORMAT, msg['addr'], msg['flags'], msg['len'])
            # Write data (if not read)
            if not (msg['flags'] & I2C_M_RD) and msg['data']:
                payload += msg['data']

        return cls.encode(ProteusMessageType.I2C_TRANSACTION, sequence, payload)

    @classmethod
    def decode_i2c_transaction(cls, payload: bytes) -> Tuple[int, List]:
        """Decode I2C transaction payload into bus id and messages"""

        MSG_COUNT_SIZE = 4

        bus_id, = struct.unpack(BUS_ID_FORMAT, payload[:BUS_ID_SIZE])
        msg_count, = struct.unpack('<I', payload[BUS_ID_SIZE: BUS_ID_SIZE + MSG_COUNT_SIZE])

        messages = []
        offset = BUS_ID_SIZE + MSG_COUNT_SIZE

        for _ in range(msg_count):
            if offset + I2C_MSG_SIZE > len(payload):
                raise ValueError("Truncated I2C message")

            addr, flags, length = struct.unpack(I2C_MSG_FORMAT, payload[offset:offset + I2C_MSG_SIZE])
            offset += I2C_MSG_SIZE

            data = b''
            if not (flags & I2C_M_RD) and length > 0:
                if offset + length > len(payload):
                    raise ValueError("Truncated I2C write data")
                data = payload[offset:offset + length]
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
