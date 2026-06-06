""" Base classes for buses and devices """

from abc import ABC, abstractmethod
from typing import Dict, Optional

from dev_proteus.devices.device_base import DeviceBase
from dev_proteus.utils.logger import get_logger


class BusBase(ABC):
    """ Base class for peripheral buses """

    def __init__(self, name: str):
        self.name = name
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
