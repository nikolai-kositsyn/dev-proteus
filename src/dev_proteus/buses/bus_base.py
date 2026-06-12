""" Base classes for buses and devices """

from abc import ABC, abstractmethod
from enum import Enum, IntEnum
from typing import Dict, Optional

from dev_proteus.devices.device_base import DeviceBase
from dev_proteus.utils.logger import get_logger


class DevType(IntEnum):
    I2C = 0
    SPI = 1
    UART = 2
    GPIO = 3
    MAX = 4


class BusBase(ABC):
    """ Base class for peripheral buses """

    def __init__(self, name: str):
        self.name = name
        self.type: DevType = DevType.MAX
        self.id: int = 0

        self.devices: Dict[int, DeviceBase] = {}
        self.logger = get_logger()

    def register_device(self, device: DeviceBase) -> None:
        """ Register a device on this bus """

        self.devices[device.address] = device

    def get_device(self, address: int) -> Optional[DeviceBase]:
        """ Get device by address """

        return self.devices.get(address)    

    @abstractmethod
    def transaction(self, context):
        """ Execute transaction on bus """
        pass
