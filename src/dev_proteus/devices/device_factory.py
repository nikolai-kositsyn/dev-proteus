from typing import Dict, Optional

from dev_proteus.devices.device_base import DeviceBase
from dev_proteus.devices.i2c.i2c_echo import I2cEcho
from dev_proteus.devices.i2c.i2c_custom_flow import I2cCustomFlow
from dev_proteus.devices.i2c.i2c_eeprom import I2cEeprom
from dev_proteus.devices.i2c.i2c_smbus import I2cSMBusDevice
from dev_proteus.devices.spi.spi_echo import SpiEcho


def create_device(address: str,
                  device_class: str,
                  name: str,
                  config: Optional[Dict] = None) -> Optional[DeviceBase]:
    """Factory function to create devices"""

    classes = {
        # I2C
        "i2c-echo": I2cEcho,
        "i2c-eeprom": I2cEeprom,
        "i2c-custom-flow": I2cCustomFlow,
        "i2c-smbus": I2cSMBusDevice,

        # SPI
        "spi-echo": SpiEcho,

    }

    if address.startswith("0x"):
        address = int(address, 16)
    else:
        address = int(address)

    target_class = classes.get(device_class.lower())
    if target_class:
        return target_class(address, name, config or {})

    return None
