""" UART bus implementation """

import re
from typing import Dict, Optional
from dev_proteus.buses.bus_base import BusBase, DevType


class UARTBus(BusBase):
    """ UART bus emulation """

    def __init__(self, name: str, config: Optional[Dict] = None):
        super().__init__(name=name)
        
        self.type = DevType.UART
        self.id = int(re.search(r'/dev/ttyS(\d+)', name).group(1))

    def transaction(self, context):
        """Execute UART transaction"""
        pass
