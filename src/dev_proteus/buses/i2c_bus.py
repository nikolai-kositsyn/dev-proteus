""" I2C bus implementation """

import re
from typing import List, Dict, Optional
from dev_proteus.buses.bus_base import BusBase, DevType
from dev_proteus.core.protocol import SMBusData


class I2CBus(BusBase):
    """ I2C bus emulation """

    def __init__(self, name: str, config: Optional[Dict] = None):
        super().__init__(name=name)

        self.type = DevType.I2C
        self.id = int(re.search(r'/dev/i2c-(\d+)', name).group(1))

        self._slave_address = 0

    def set_slave_address(self, slave_address: int) -> None:
        self._slave_address = slave_address

    def smbus_transaction(self, request_data: SMBusData) -> Optional[SMBusData]:
        """ Execute SMBus transaction """

        device = self.get_device(self._slave_address)
        if device:
            return device.smbus_transaction(request_data)
        else:
            raise Exception(f"[{self.name}] Couldn't find slave 0x{self._slave_address:04X} to handle SMBus")

    def transaction(self, context) -> bytes:
        """ Execute I2C transaction """

        messages: List[Dict] = context
        results: bytearray = []

        for msg in messages:
            addr = msg.get('addr')
            is_read = msg.get('is_read', False)
            length = msg.get('length', 0)
            data = msg.get('data', b'')

            device = self.get_device(addr)

            if not device:
                self.logger.warning(f"[{self.name}] No device at address 0x{addr:04X}")
                continue

            if is_read:
                result = device.read(length)
                results.extend(result)
                self.logger.debug(f"[{self.name}] Read {len(result)} bytes from 0x{addr:04X}")
            else:
                device.write(data)
                self.logger.debug(f"[{self.name}] Written {len(data)} bytes to 0x{addr:04X}")

        return bytes(results)
