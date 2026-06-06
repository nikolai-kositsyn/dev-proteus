""" GPIO bus implementation """

from typing import Dict, Optional
from dev_proteus.buses.bus_base import BusBase


class GPIOBus(BusBase):
    """ GPIO bus emulation """

    def __init__(self, bus_id: int, config: Optional[Dict] = None):
        super().__init__(f"gpio-{bus_id}")
        self.bus_id = bus_id

    def transaction(self, context):
        """ Execute GPIO transaction """
        pass
