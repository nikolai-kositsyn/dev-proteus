""" GPIO bus implementation """

import re
from typing import Dict, Optional
from dev_proteus.buses.bus_base import BusBase, DevType


class GPIOBus(BusBase):
    """ GPIO bus emulation """

    def __init__(self, name: str, config: Optional[Dict] = None):
        super().__init__(name=name)
        
        self.type = DevType.UART
        self.id = int(re.search(r'/dev/gpiochip(\d+)', name).group(1))

    def transaction(self, context):
        """ Execute GPIO transaction """
        pass
