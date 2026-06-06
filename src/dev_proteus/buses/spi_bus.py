"""SPI bus implementation"""

from typing import Dict, Optional
from dev_proteus.buses.bus_base import BusBase


class SPIBus(BusBase):
    """SPI bus emulation"""

    def __init__(self, bus_id: int, config: Optional[Dict] = None):
        super().__init__(f"spi-{bus_id}")
        self.bus_id = bus_id
        self.mode = config.get('mode', 0) if config else 0
        self.speed = config.get('speed', 1000000) if config else 1000000
        self.bits_per_word = config.get('bits_per_word', 8) if config else 8

    def transaction(self, cs: int, tx_data: bytes) -> bytes:
        """Execute SPI transaction"""
        device = self.get_device(cs)

        if not device:
            self.logger.warning(f"No device at CS {cs}")
            return b'\x00' * len(tx_data)

        # SPI devices usually process tx and produce rx simultaneously
        return device.transfer(tx_data)
