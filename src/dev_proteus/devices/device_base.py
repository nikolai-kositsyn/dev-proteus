""" Base device implementation """

from abc import ABC, abstractmethod
from typing import Dict, Optional
from dev_proteus.core.protocol import SMBusData, SpiTransfer
from dev_proteus.utils.logger import get_logger


class DeviceBase(ABC):
    """ Base class for all emulated devices """

    def __init__(self, address: int, name: str, config: Optional[Dict] = None):
        self.address = address
        self.name = name
        self.config = config or {}
        self.logger = get_logger()

    @abstractmethod
    def write(self, data: bytes) -> None:
        """ Handle write operation """
        pass

    @abstractmethod
    def read(self, length: int) -> bytes:
        """ Handle read operation """
        pass


class I2CDeviceBase(DeviceBase):
    """ Base class for I2C devices """

    def __init__(self, address: int, name: str, config: Optional[Dict] = None):
        super().__init__(address, name, config)

    def smbus_transaction(self, request_data: SMBusData) -> Optional[SMBusData]:
        raise NotImplementedError(f"{self.name}")


class SPIDeviceBase(DeviceBase):
    """ Base class for SPI devices """

    def __init__(self, cs: int, name: str, config: Optional[Dict] = None):
        super().__init__(cs, name, config)
        self.cs = cs

    @abstractmethod
    def transfer(self, transfer: SpiTransfer) -> bytes:
        """ Handle SPI transfer """
        pass
