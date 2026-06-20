""" UART echo device emulator """

from typing import Dict, Optional
from dev_proteus.devices.device_base import UARTDeviceBase


class UartEcho(UARTDeviceBase):

    def __init__(self, cs: int, name: str, config: Optional[Dict] = None):
        super().__init__(cs, name, config)

        self._data = b''

    def read(self, length: int) -> bytes:
        """ Return internal bytes """

        data_to_return: bytes = []

        data_length = len(self._data)
        if data_length >= length:
            data_to_return = self._data[:length]
        else:
            delta_length = length - data_length
            data_to_return = self._data[:data_length] + bytes([0xFF] * delta_length)

        self.logger.debug(f"[{self.name}] Read   : {data_to_return.hex().upper()}")
        return data_to_return

    def write(self, data: bytes) -> None:
        """ Update internal bytes """

        self._data = data
        self.logger.debug(f"[{self.name}] Written: {self._data.hex().upper()}")    
    
    def get_available_bytes(self) -> int:
        return len(self._data)