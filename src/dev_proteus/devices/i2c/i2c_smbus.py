"""SMBus device emulator"""

from typing import Dict, Optional

from dev_proteus.core.protocol import *
from dev_proteus.devices.device_base import I2CDeviceBase


class I2cSMBusDevice(I2CDeviceBase):
    """SMBus device emulator"""

    def __init__(self, address: int, name: str, config: Optional[Dict] = None):
        super().__init__(address, name, config)

        # Initialize registers
        self.registers = {}
        self.block_registers = {}  # For block data
        self._init_registers()

    def _init_registers(self):
        """Initialize device registers from config or defaults"""
        if self.config and 'registers' in self.config:
            for reg, value in self.config['registers'].items():
                reg_int = int(reg, 16) if isinstance(reg, str) else reg
                if isinstance(value, list):
                    self.block_registers[reg_int] = bytes(value)
                else:
                    self.registers[reg_int] = value
        else:
            # Default test registers for SMBus client
            self.registers = {
                0x00: 0x00,  # Device ID
                0x10: 0x00,  # Byte Data register
                0x20: 0x0000,  # Word Data register (16-bit)
                0x30: 0xABCD,  # Process Call register
            }
            self.block_registers = {
                0x40: bytes([0x01, 0x02, 0x03, 0x04, 0x05]),  # Block data
                0x50: bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF]),  # I2C Block data
            }

    def _get_data_bytes(self, data: SMBusData) -> bytes:
        """Extract data bytes from SMBusData block"""
        if not data.block or len(data.block) == 0:
            return b''
        data_len = data.block[0]
        if data_len > SMBUS_BLOCK_MAX:
            data_len = SMBUS_BLOCK_MAX
        return data.block[1:1 + data_len]

    def _create_response_block(self, data_bytes: bytes) -> bytes:
        """Create response block from data bytes"""
        block = bytearray(SMBUS_BLOCK_SIZE)
        data_len = min(len(data_bytes), SMBUS_BLOCK_MAX)
        block[0] = data_len
        block[1:1 + data_len] = data_bytes[:data_len]
        return bytes(block)

    def _handle_quick(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_QUICK"""
        if data.read_write == I2C_SMBUS_READ:
            self.logger.debug("Quick Read - device present")
        else:
            self.logger.debug("Quick Write - device present")
        return b''  # No data

    def _handle_byte(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_BYTE"""
        if data.read_write == I2C_SMBUS_READ:
            # Receive Byte: read one byte
            value = self.registers.get(0, 0x00)
            self.logger.debug(f"Receive Byte: 0x{value:02X}")
            return bytes([value & 0xFF])
        else:
            # Send Byte: write one byte (command is the value)
            value = data.command
            self.registers[0] = value
            self.logger.debug(f"Send Byte: 0x{value:02X}")
            return b''

    def _handle_byte_data(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_BYTE_DATA"""
        if data.read_write == I2C_SMBUS_READ:
            # Read Byte Data
            value = self.registers.get(data.command, 0x00)
            self.logger.debug(f"Read Byte Data: reg=0x{data.command:02X}, value=0x{value:02X}")
            return bytes([value & 0xFF])
        else:
            # Write Byte Data
            data_bytes = self._get_data_bytes(data)
            if data_bytes:
                value = data_bytes[0]
                self.registers[data.command] = value
                self.logger.debug(f"Write Byte Data: reg=0x{data.command:02X}, value=0x{value:02X}")
            return b''

    def _handle_word_data(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_WORD_DATA"""
        if data.read_write == I2C_SMBUS_READ:
            # Read Word Data
            value = self.registers.get(data.command, 0x0000)
            self.logger.debug(f"Read Word Data: reg=0x{data.command:02X}, value=0x{value:04X}")
            # Return as big-endian (MSB first)
            return bytes([(value >> 8) & 0xFF, value & 0xFF])
        else:
            # Write Word Data
            data_bytes = self._get_data_bytes(data)
            if len(data_bytes) >= 2:
                value = (data_bytes[0] << 8) | data_bytes[1]
                self.registers[data.command] = value
                self.logger.debug(f"Write Word Data: reg=0x{data.command:02X}, value=0x{value:04X}")
            return b''

    def _handle_process_call(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_PROC_CALL"""
        # Write Word
        data_bytes = self._get_data_bytes(data)
        if len(data_bytes) >= 2:
            value = (data_bytes[0] << 8) | data_bytes[1]
            self.registers[data.command] = value
            self.logger.debug(f"Process Call Write: reg=0x{data.command:02X}, value=0x{value:04X}")

        # Read Word (same register)
        result = self.registers.get(data.command, 0x0000)
        self.logger.debug(f"Process Call Read: reg=0x{data.command:02X}, result=0x{result:04X}")
        return bytes([(result >> 8) & 0xFF, result & 0xFF])

    def _handle_block_data(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_BLOCK_DATA"""
        if data.read_write == I2C_SMBUS_READ:
            # Read Block Data
            if data.command in self.block_registers:
                response = self.block_registers[data.command]
            else:
                # Default response with some test data
                response = bytes([0x01, 0x02, 0x03, 0x04, 0x05])

            self.logger.debug(f"Read Block Data: reg=0x{data.command:02X}, len={len(response)}")
            return response
        else:
            # Write Block Data
            data_bytes = self._get_data_bytes(data)
            self.block_registers[data.command] = data_bytes
            self.logger.debug(f"Write Block Data: reg=0x{data.command:02X}, len={len(data_bytes)}")
            return b''

    def _handle_i2c_block_data(self, data: SMBusData) -> Optional[bytes]:
        """Handle I2C_SMBUS_I2C_BLOCK_DATA"""
        if data.read_write == I2C_SMBUS_READ:
            # Read I2C Block Data
            if data.command in self.block_registers:
                response = self.block_registers[data.command]
            else:
                # Default response
                response = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])

            self.logger.debug(f"Read I2C Block Data: reg=0x{data.command:02X}, len={len(response)}")
            return response
        else:
            # Write I2C Block Data
            data_bytes = self._get_data_bytes(data)
            self.block_registers[data.command] = data_bytes
            self.logger.debug(f"Write I2C Block Data: reg=0x{data.command:02X}, len={len(data_bytes)}")
            return b''

    # ========== Main dispatcher ==========

    def smbus_transaction(self, request_data: SMBusData) -> Optional[SMBusData]:
        """
        Handle SMBus transaction and return response data
        """
        self.logger.debug(f"SMBus: rw={request_data.read_write}, cmd=0x{request_data.command:02X}, "
                          f"size={request_data.size}, block={request_data.block.hex()}")

        # Dispatch based on transaction type
        handlers = {
            I2C_SMBUS_QUICK: self._handle_quick,
            I2C_SMBUS_BYTE: self._handle_byte,
            I2C_SMBUS_BYTE_DATA: self._handle_byte_data,
            I2C_SMBUS_WORD_DATA: self._handle_word_data,
            I2C_SMBUS_PROC_CALL: self._handle_process_call,
            I2C_SMBUS_BLOCK_DATA: self._handle_block_data,
            I2C_SMBUS_I2C_BLOCK_DATA: self._handle_i2c_block_data,
        }

        handler = handlers.get(request_data.size)
        if not handler:
            self.logger.warning(f"Unsupported SMBus transaction size: {request_data.size}")
            return None

        response_data = handler(request_data)

        # For READ operations that return data, create response
        if response_data is not None and len(response_data) > 0:
            response_block = self._create_response_block(response_data)
            result = SMBusData(
                read_write=I2C_SMBUS_READ,
                command=request_data.command,
                size=request_data.size,
                block=response_block
            )
            self.logger.debug(f"Response: block={result.block.hex()}")
            return result

        # For WRITE operations or READ with no data, return None
        return None

    # ========== I2CDeviceBase interface ==========

    def read(self, length: int) -> bytes:
        """Raw I2C read"""
        return b'\x00' * length

    def write(self, data: bytes) -> None:
        """Raw I2C write"""
        pass
