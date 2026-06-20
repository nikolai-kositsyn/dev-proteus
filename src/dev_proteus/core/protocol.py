""" Protocol definitions for communication between hook library and emulator """

from dataclasses import dataclass
import struct
from enum import IntEnum
from typing import Dict, List, Tuple

# Protocol constants
PROTEUS_MAGIC = 0x50524F54  # "PROT"


# Commands
class ProteusCommand(IntEnum):
    # General
    GET_DEVICES = 0

    # I2C / SMBus
    I2C_SET_SLAVE = 10
    I2C_TRANSACTION = 11
    SMBUS_TRANSACTION = 12

    # SPI
    SPI_SET_MODE = 20
    SPI_GET_MODE = 21

    SPI_SET_BITS_PER_WORD = 22
    SPI_GET_BITS_PER_WORD = 23

    SPI_SET_MAX_SPEED_HZ = 24
    SPI_GET_MAX_SPEED_HZ = 25

    SPI_SET_LSB_FIRST = 26
    SPI_GET_LSB_FIRST = 27

    SPI_SET_MODE32 = 28
    SPI_GET_MODE32 = 29

    SPI_TRANSACTION = 30

    # UART
    UART_SET_TERMIOS = 40
    UART_GET_TERMIOS = 41
    UART_SET_MODEM = 42
    UART_GET_MODEM = 43
    UART_READ = 44
    UART_WRITE = 45
    UART_GET_AVAILABLE_BYTES = 46

    # GPIO
    GPIO_READ = 50
    GPIO_WRITE = 51
    GPIO_DIRECTION = 52


# Status codes
class ProteusStatus(IntEnum):
    SUCCESS = 0
    DEVICE_NOT_FOUND = 1
    WRONG_INPUT = 2
    COMMAND_FAILED = 3
    COMMAND_NOT_FOUND = 4


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

# Device/Bus info: type (1B), name (255B)
DEVICE_NAME_LEN = 255
COUNT_OF_DEVICES_FORMAT = '<H'
DEVICE_INFO_FORMAT = '<B255s'
DEVICE_INFO_SIZE = struct.calcsize(DEVICE_INFO_FORMAT)

"""
I2C
"""

# I2C Transaction header: bus id, message count
I2C_TRANSACTION_HEADER_FORMAT = "<II"
I2C_TRANSACTION_HEADER_SIZE = struct.calcsize(I2C_TRANSACTION_HEADER_FORMAT)

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

# SPI Transaction header: bus id, cs, transfers count
SPI_TRANSACTION_HEADER_FORMAT = "<IBI"
SPI_TRANSACTION_HEADER_SIZE = struct.calcsize(SPI_TRANSACTION_HEADER_FORMAT)

# SPI Transfer header
SPI_TRANSFER_HEADER_FORMAT = "<BBIIHBBBBB"
SPI_TRANSFER_HEADER_SIZE = struct.calcsize(SPI_TRANSFER_HEADER_FORMAT)


@dataclass
class SpiTransfer:
    tx_buf: bytes
    rx_buf: bytes
    len: int
    speed_hz: int
    delay_usecs: int
    bits_per_word: int
    cs_change: int
    tx_nbits: int
    rx_nbits: int
    word_delay_usecs: int


"""
UART
"""

# UART set/get termios
UART_TERMIOS_FORMAT = "<IIIIB3x32sII" # 60 bytes
UART_TERMIOS_SIZE = struct.calcsize(UART_TERMIOS_FORMAT)

UART_MODEM_FORMAT = "<I"
UART_MODEM_SIZE = struct.calcsize(UART_MODEM_FORMAT)

UART_RW_SIZE_FORMAT = "<H"
UART_RW_SIZE_SIZE = struct.calcsize(UART_RW_SIZE_FORMAT)


@dataclass
class UartTermios:    
    c_iflag: int
    c_oflag: int
    c_cflag: int
    c_lflag: int
    c_line: int
    c_cc: bytes
    c_ispeed: int
    c_ospeed: int


class ProtocolMessage:
    """Protocol message builder/parser"""

    """
    General
    """

    @classmethod
    def decode_bus_id(cls, req_payload: bytes) -> int:
        """ Decode Bus Id from transaction payload """

        if len(req_payload) < BUS_ID_SIZE:
            raise ValueError("Invalid 'Bus Id' in transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        return bus_id

    """
    I2C
    """

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
        """ Decode I2C transaction payload into bus id and messages """

        bus_id, msg_count = struct.unpack(I2C_TRANSACTION_HEADER_FORMAT, req_payload[:I2C_TRANSACTION_HEADER_SIZE])

        messages = []
        offset = I2C_TRANSACTION_HEADER_SIZE

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

    """
    SPI
    """

    @classmethod
    def decode_spi_set_mode(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + 1:
            raise ValueError("Invalid 'Set SPI Mode' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        mode, = struct.unpack("<B", req_payload[BUS_ID_SIZE:BUS_ID_SIZE + 1])

        return bus_id, mode

    @classmethod
    def decode_spi_set_bits_per_word(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + 1:
            raise ValueError("Invalid 'Set SPI Bits Per Word' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        bits_per_word, = struct.unpack("<B", req_payload[BUS_ID_SIZE:BUS_ID_SIZE + 1])

        return bus_id, bits_per_word

    @classmethod
    def decode_spi_set_max_speed_hz(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + 4:
            raise ValueError("Invalid 'Set SPI Max Speed Hz' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        max_speed_hz, = struct.unpack("<I", req_payload[BUS_ID_SIZE:BUS_ID_SIZE + 4])

        return bus_id, max_speed_hz

    @classmethod
    def decode_spi_set_lsb_first(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + 1:
            raise ValueError("Invalid 'Set SPI LSB First' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        lsb_first, = struct.unpack("<B", req_payload[BUS_ID_SIZE:BUS_ID_SIZE + 1])

        return bus_id, lsb_first

    @classmethod
    def decode_spi_set_mode32(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + 4:
            raise ValueError("Invalid 'Set SPI Mode32' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        mode32, = struct.unpack("<I", req_payload[BUS_ID_SIZE:BUS_ID_SIZE + 4])

        return bus_id, mode32

    @classmethod
    def decode_spi_transaction(cls, req_payload: bytes) -> Tuple[int, int, List[SpiTransfer]]:
        """ Decode SPI transaction payload into bus id, cs id and transfers """

        bus_id, cs_id, trans_count = struct.unpack(SPI_TRANSACTION_HEADER_FORMAT,
                                                   req_payload[:SPI_TRANSACTION_HEADER_SIZE])

        transfers: List[SpiTransfer] = []
        offset = SPI_TRANSACTION_HEADER_SIZE

        for _ in range(trans_count):
            if offset + SPI_TRANSFER_HEADER_SIZE > len(req_payload):
                raise ValueError("Truncated SPI transaction")

            is_tx, is_rx, length, speed_hz, delay_usecs, bits_per_word, cs_change, tx_nbits, rx_nbits, word_delay_usecs = struct.unpack(
                SPI_TRANSFER_HEADER_FORMAT, req_payload[offset:offset + SPI_TRANSFER_HEADER_SIZE])
            offset += SPI_TRANSFER_HEADER_SIZE

            tx_buf = None
            if is_tx and length > 0:
                if offset + length > len(req_payload):
                    raise ValueError("Truncated SPI data to write")
                tx_buf = req_payload[offset:offset + length]
                offset += length

            rx_buf = bytes() if is_rx else None

            transfers.append(SpiTransfer(tx_buf=tx_buf,
                                         rx_buf=rx_buf,
                                         len=length,
                                         speed_hz=speed_hz,
                                         delay_usecs=delay_usecs,
                                         bits_per_word=bits_per_word,
                                         cs_change=cs_change,
                                         tx_nbits=tx_nbits,
                                         rx_nbits=rx_nbits,
                                         word_delay_usecs=word_delay_usecs))
        return bus_id, cs_id, transfers
    
    """
    UART
    """

    @classmethod
    def decode_uart_set_termios(cls, req_payload: bytes) -> Tuple[int, UartTermios]:
        if len(req_payload) < BUS_ID_SIZE + UART_TERMIOS_SIZE:
            raise ValueError("Invalid 'Set UART termios' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        c_iflag, c_oflag, c_cflag, c_lflag, c_line, c_cc, c_ispeed, c_ospeed, = struct.unpack(UART_TERMIOS_FORMAT, 
                                                                                      req_payload[BUS_ID_SIZE:BUS_ID_SIZE + UART_TERMIOS_SIZE])

        return bus_id, UartTermios(c_iflag=c_iflag, c_oflag=c_oflag, c_cflag=c_cflag, c_lflag=c_lflag,
                                   c_line=c_line, c_cc=c_cc,
                                   c_ispeed=c_ispeed, c_ospeed=c_ospeed)
    
    @classmethod
    def encode_uart_termios(cls, termios: UartTermios) -> bytes:
        payload = struct.pack(UART_TERMIOS_FORMAT,
                              termios.c_iflag, termios.c_oflag, termios.c_cflag, termios.c_lflag,
                              termios.c_line, termios.c_cc,
                              termios.c_ispeed, termios.c_ospeed)
        return payload

    @classmethod
    def decode_uart_set_modem(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + UART_MODEM_SIZE:
            raise ValueError("Invalid 'Set UART modem' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        status, = struct.unpack(UART_MODEM_FORMAT, req_payload[BUS_ID_SIZE:BUS_ID_SIZE + UART_MODEM_SIZE])

        return bus_id, status
    
    @classmethod
    def encode_uart_modem(cls, status: int) -> bytes:
        payload = struct.pack(UART_MODEM_FORMAT, status)
        return payload
    
    @classmethod
    def decode_uart_write(cls, req_payload: bytes) -> Tuple[int, bytes]:
        if len(req_payload) < BUS_ID_SIZE:
            raise ValueError("Invalid 'Write UART data' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        data_to_write = req_payload[BUS_ID_SIZE:]

        return bus_id, data_to_write
    
    @classmethod
    def decode_uart_read(cls, req_payload: bytes) -> Tuple[int, int]:
        if len(req_payload) < BUS_ID_SIZE + UART_RW_SIZE_SIZE:
            raise ValueError("Invalid 'Read UART data' transaction payload")

        bus_id, = struct.unpack(BUS_ID_FORMAT, req_payload[:BUS_ID_SIZE])
        size_to_read, = struct.unpack(UART_RW_SIZE_FORMAT, req_payload[BUS_ID_SIZE:BUS_ID_SIZE + UART_RW_SIZE_SIZE])

        return bus_id, size_to_read
