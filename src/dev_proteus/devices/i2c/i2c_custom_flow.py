""" I2C custom flow device emulator """

import json
import re
from typing import Dict, Optional, List
from collections import deque

from dev_proteus.devices.device_base import I2CDeviceBase


class Exchange:
    """Represents a single I2C exchange with request/response/delay"""

    def __init__(self, request: str, response: str, delay: int = 0):
        self.request = self._parse_hex_string(request)
        self.response = self._parse_hex_string(response)
        self.delay = delay
        self.completed = False

    def _parse_hex_string(self, hex_str: str) -> bytes:
        """Parse hex string like '7E 000000000000 0000 00 01C1' to bytes"""
        # Remove all whitespace and split
        hex_str = hex_str.strip()
        # Replace multiple spaces/tabs with single space
        hex_str = re.sub(r'\s+', ' ', hex_str)
        # Split by space and handle each part
        parts = hex_str.split()

        result = []
        for part in parts:
            # Handle continuous hex strings (like '01C1')
            if len(part) > 2:
                # Split into 2-character chunks
                for i in range(0, len(part), 2):
                    chunk = part[i:i + 2]
                    if len(chunk) == 2:
                        try:
                            result.append(int(chunk, 16))
                        except ValueError:
                            print(f"Warning: Invalid hex chunk '{chunk}' in '{hex_str}'")
            else:
                # Single byte
                try:
                    result.append(int(part, 16))
                except ValueError:
                    print(f"Warning: Invalid hex byte '{part}' in '{hex_str}'")

        return bytes(result)

    def matches(self, request_bytes: bytes) -> bool:
        """Check if incoming request matches expected request"""
        if self.completed:
            return False

        # Compare length
        if len(request_bytes) != len(self.request):
            return False

        # Compare content
        for i in range(len(request_bytes)):
            if request_bytes[i] != self.request[i]:
                return False

        return True

    def mark_completed(self):
        """Mark exchange as completed"""
        self.completed = True

    def __repr__(self):
        return f"Exchange(request={self.request.hex()}, response={self.response.hex()}, delay={self.delay})"


class I2cCustomFlow(I2CDeviceBase):

    def __init__(self, address: int, name: str, config: Optional[Dict] = None):
        super().__init__(address, name, config)

        self._exchanges_queue = deque()
        self._active_response = b''
        self._current_exchange = None

        self.reset()

    def _parse_flow(self, flow_config: List[Dict]):
        """Parse flow from JSON configuration"""
        for exchange_config in flow_config:
            request = exchange_config.get('request', '')
            response = exchange_config.get('response', '')
            delay = exchange_config.get('delay', 0)

            if not request or not response:
                self.logger.warning(f"Skipping exchange: missing request or response")
                continue

            try:
                exchange = Exchange(request, response, delay)
                self._exchanges_queue.append(exchange)
                # self.logger.debug(f"Loaded exchange: {exchange}")
            except Exception as e:
                self.logger.error(f"Failed to parse exchange: {e}")

    def _get_next_exchange(self) -> Optional[Exchange]:
        """Get next pending exchange from queue"""
        # Clean up completed exchanges from front
        while self._exchanges_queue and self._exchanges_queue[0].completed:
            self._exchanges_queue.popleft()

        # Return next pending exchange
        if self._exchanges_queue:
            return self._exchanges_queue[0]
        return None

    def write(self, request_bytes: bytes) -> None:
        """ 
        1. Get next exchange from queue
        2. Validate expected request
        3. Keep active response frame
        """
        self.logger.debug(f"Write request: {request_bytes.hex()}")

        # Get next exchange
        exchange = self._get_next_exchange()

        if not exchange:
            self.logger.warning(f"No pending exchange for request: {request_bytes.hex()}")
            self._active_response = b''
            return

        # Validate request matches expected
        if not exchange.matches(request_bytes):
            self.logger.error(f"Request mismatch! Expected: {exchange.request.hex()}, Got: {request_bytes.hex()}")
            self._active_response = b''
            return

        # Check delay
        if exchange.delay > 0:
            import time
            self.logger.debug(f"Waiting {exchange.delay}ms before response")
            time.sleep(exchange.delay / 1000.0)

        # Set active response frame
        self._active_response = exchange.response
        self._current_exchange = exchange

        self.logger.debug(f"Activated response: {self._active_response.hex()}")

    def read(self, length: int) -> bytes:
        """ 
        1. Check active response frame
        2. Return the next part of response (required length is count of bytes to return)
        """
        if not self._active_response:
            self.logger.warning(f"Read requested but no active response frame")
            return b''
            return b'\xFF' * length

        # Return requested number of bytes from active response
        response_part = self._active_response[:length]
        self._active_response = self._active_response[length:]

        self.logger.debug(f"Read {len(response_part)} bytes: {response_part.hex()}")

        # If response is fully consumed, mark exchange as completed
        if not self._active_response and self._current_exchange:
            self.logger.debug(f"Exchange completed")
            self._current_exchange.mark_completed()
            self._current_exchange = None

        # If we don't have enough bytes, pad with zeros
        if len(response_part) < length:
            padding = b'\xFF' * (length - len(response_part))
            response_part += padding
            self.logger.warning(f"Response exhausted, padding with {len(padding)} 0xFF")

        return response_part

    def reset(self):
        """Reset device state"""
        self._exchanges_queue.clear()
        self._active_response = b''
        self._current_exchange = None

        # Parse flow from config
        if self.config and 'flow' in self.config:
            self._parse_flow(self.config['flow'])
            self.logger.info(f"Loaded {len(self._exchanges_queue)} exchanges for '{self.name}'")
        else:
            self.logger.warning(f"No flow configured for '{self.name}'")
