"""UART bus implementation"""

from typing import Dict, Optional
from dev_proteus.buses.bus_base import BusBase


class UARTBus(BusBase):
    """UART bus emulation"""

    def __init__(self, bus_id: int, config: Optional[Dict] = None):
        super().__init__(f"uart-{bus_id}")
        self.bus_id = bus_id

    def transaction(self, context):
        """Execute UART transaction"""
        pass
