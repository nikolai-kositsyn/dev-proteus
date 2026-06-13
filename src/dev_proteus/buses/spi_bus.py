""" SPI bus implementation """

import re
from typing import Dict, List, Optional
from dev_proteus.buses.bus_base import BusBase, DevType
from dev_proteus.core.protocol import SpiTransfer


class SPIBus(BusBase):
    """ SPI bus emulation """

    def __init__(self, name: str, config: Optional[Dict] = None):
        super().__init__(name=name)
        
        self.type = DevType.SPI
        id, cs = map(int, re.search(r'/dev/spidev(\d+)\.(\d+)', name).groups())
        self.id = id
        self.cs = cs        
        
        self.mode = config.get('mode', 0) if config else 0
        self.bits_per_word = config.get('bits_per_word', 0) if config else 0
        self.max_speed_hz = config.get('max_speed_hz', 0) if config else 0        
        self.lsb_first = config.get('lsb_first', 0) if config else 0

    def transaction(self, context: Dict) -> bytes:
        """ Execute SPI transaction """        

        cs: int = context.get("cs", self.cs)
        transfers: List[SpiTransfer] = context.get("transfers")
    
        device = self.get_device(cs)
        if not device:
            self.logger.warning(f"Couldn't find SPI device with CS={cs} on bus {self.name}")
            return None
        
        total_result = bytearray()

        for transfer in transfers:
            transfer_result = device.transfer(transfer)
            total_result.extend(transfer_result)

        return bytes(total_result)
