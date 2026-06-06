""" EEPROM emulator (24LC256-like) """

import os
import time
from typing import Dict, Optional
from dev_proteus.devices.device_base import I2CDeviceBase


class I2cEeprom(I2CDeviceBase):
    """24LC256 compatible EEPROM emulator with proper page write and write cycle timing"""

    # 24LC256 constants
    PAGE_SIZE = 64  # Bytes per page
    SIZE_BYTES = 32768  # 32KB = 256Kbit
    WRITE_CYCLE_MS = 5  # Typical write cycle time (max 5ms)

    def __init__(self, address: int, name: str, config: Optional[Dict] = None):
        super().__init__(address, name, config)

        # Configuration
        self.size = config.get('size', self.SIZE_BYTES)
        self.persistent_file = config.get('persistent_file')
        self.simulate_write_delay = config.get('simulate_write_delay', True)

        # State
        self.memory = bytearray(self.size)

        self._reset()
        self._init_memory()

    def _reset(self):
        """Reset device to initial state"""
        self.current_address = 0
        self._write_in_progress = False
        self._write_until_time = 0

    def _init_memory(self):
        """Initialize or load memory from persistent storage"""
        if self.persistent_file and os.path.exists(self.persistent_file):
            try:
                with open(self.persistent_file, 'rb') as f:
                    data = f.read()
                    # Copy loaded data into memory
                    copy_len = min(len(data), self.size)
                    self.memory[:copy_len] = data[:copy_len]
                    # Fill remaining with 0xFF
                    if copy_len < self.size:
                        self.memory[copy_len:] = b'\xFF' * (self.size - copy_len)
                self.logger.info(f"[{self.name}] Loaded {copy_len} bytes from {self.persistent_file}")
            except Exception as e:
                self.logger.error(f"[{self.name}] Failed to load persistent file: {e}")
                self._init_with_pattern()
        else:
            self._init_with_pattern()

    def _init_with_pattern(self):
        """Initialize memory with test pattern"""
        for i in range(min(256, self.size)):
            self.memory[i] = i & 0xFF
        # Fill rest with 0xFF
        if self.size > 256:
            self.memory[256:] = b'\xFF' * (self.size - 256)
        self.logger.debug(f"[{self.name}] Initialized memory with test pattern")

    def _save_persistent(self):
        """Save memory to persistent file"""
        if self.persistent_file:
            try:
                # Create directory if needed
                os.makedirs(os.path.dirname(self.persistent_file), exist_ok=True)
                with open(self.persistent_file, 'wb') as f:
                    f.write(self.memory)
                self.logger.debug(f"[{self.name}] Saved {self.size} bytes to {self.persistent_file}")
            except Exception as e:
                self.logger.error(f"[{self.name}] Failed to save persistent file: {e}")

    def _wait_for_write_cycle(self):
        """Wait if a write cycle is in progress"""
        if self._write_in_progress:
            remaining = self._write_until_time - time.monotonic()
            if remaining > 0:
                self.logger.debug(f"[{self.name}] Waiting {remaining * 1000:.1f}ms for write cycle")
                time.sleep(remaining)
            self._write_in_progress = False

    def _start_write_cycle(self):
        """Start a write cycle (simulates EEPROM programming time)"""
        if self.simulate_write_delay:
            self._write_in_progress = True
            self._write_until_time = time.monotonic() + (self.WRITE_CYCLE_MS / 1000.0)

    def _page_write(self, start_addr: int, data: bytes) -> int:
        """
        Perform page write (wraps within page boundaries)
        Returns number of bytes written
        """
        # Calculate page boundaries
        page_start = (start_addr // self.PAGE_SIZE) * self.PAGE_SIZE
        page_end = page_start + self.PAGE_SIZE

        # Determine how many bytes we can write without crossing page boundary
        max_bytes = page_end - start_addr
        bytes_to_write = min(len(data), max_bytes, self.size - start_addr)

        if bytes_to_write <= 0:
            return 0

        # Write data
        for i in range(bytes_to_write):
            if start_addr + i < self.size:
                self.memory[start_addr + i] = data[i]

        return bytes_to_write

    def write(self, data: bytes) -> None:
        """
        Handle EEPROM write operation.
        24LC256 protocol:
        - 2 bytes: set address (MSB first)
        - >2 bytes: sequential write (page write)
        """
        self._wait_for_write_cycle()

        if len(data) == 0:
            return

        if len(data) == 2:
            # Set address (no data write)
            self.current_address = (data[0] << 8) | data[1]
            self.logger.debug(f"[{self.name}] Address set to 0x{self.current_address:04X}")
            return

        # Handle write operations
        if len(data) == 3:
            # Single byte write: address (2 bytes) + data (1 byte)
            address = (data[0] << 8) | data[1]
            value = data[2]

            if address < self.size:
                self.memory[address] = value
                self.current_address = address + 1
                self.logger.debug(f"[{self.name}] Written 0x{value:02X} to 0x{address:04X}")
            else:
                self.logger.warning(f"[{self.name}] Write address 0x{address:04X} out of range")

            self._start_write_cycle()
            self._save_persistent()

        elif len(data) > 3:
            # Page write: address (2 bytes) + data bytes
            address = (data[0] << 8) | data[1]
            write_data = data[2:]

            bytes_written = self._page_write(address, write_data)
            self.current_address = address + bytes_written

            if bytes_written < len(write_data):
                self.logger.warning(
                    f"[{self.name}] Page write truncated: wrote {bytes_written} of {len(write_data)} bytes")

            self.logger.debug(f"[{self.name}] Page wrote {bytes_written} bytes starting at 0x{address:04X}")
            self._start_write_cycle()
            self._save_persistent()
        else:
            # len(data) == 1? Should not happen for 24LC256
            self.logger.warning(f"[{self.name}] Unexpected write length: {len(data)} bytes")

    def read(self, length: int) -> bytes:
        """
        Read data from current address.
        After address is set, EEPROM increments address automatically on each read.
        """
        self._wait_for_write_cycle()

        if length <= 0:
            return b''

        # Read from current address
        end_addr = min(self.current_address + length, self.size)
        data = self.memory[self.current_address:end_addr]

        # If we tried to read beyond memory, pad with 0xFF
        if len(data) < length:
            data += b'\xFF' * (length - len(data))

        read_len = len(data)
        self.logger.debug(f"[{self.name}] Read {read_len} bytes from 0x{self.current_address:04X}")

        # Auto-increment address
        self.current_address = (self.current_address + read_len) % self.size

        return bytes(data)
