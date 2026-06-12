"""Main emulator server"""

import re
import socket
import threading
from typing import Dict, Optional
import struct

from dev_proteus.core.protocol import *
from dev_proteus.buses.bus_base import BusBase, DevType
from dev_proteus.buses.i2c_bus import I2CBus
from dev_proteus.buses.spi_bus import SPIBus
from dev_proteus.buses.uart_bus import UARTBus
from dev_proteus.buses.gpio_bus import GPIOBus
from dev_proteus.devices.device_factory import create_device
from dev_proteus.utils.logger import get_logger
from dev_proteus.utils.config_loader import load_config

HOST_DEFAULT = "0.0.0.0"
PORT_DEFAULT = 4242


class EmulatorServer:
    """Main emulator server handling all peripheral emulation"""

    def __init__(self, config_path: Optional[str] = None, host: str = HOST_DEFAULT, port: int = PORT_DEFAULT):
        self.host = host
        self.port = port
        self.config_path = config_path
        self.buses: List[BusBase] = []
        self.socket: Optional[socket.socket] = None
        self.running = False
        self.logger = get_logger()

    def load_config(self) -> None:
        """Load configuration from JSON file"""
        if not self.config_path:
            self.logger.warning("No config file provided")
            return

        config = load_config(self.config_path)

        for bus_item in config.get('buses', []):
            bus_name = bus_item.get('name')
            bus_config = bus_item.get('config', {})      

            if "i2c" in bus_name:                
                bus = I2CBus(bus_name, bus_config)

            elif "spi" in bus_name:                
                bus = SPIBus(bus_name, bus_config)

            elif "tty" in bus_name:                
                bus = UARTBus(bus_name, bus_config)

            elif "gpiochip" in bus_name:                
                bus = GPIOBus(bus_name, bus_config)

            else:
                self.logger.warning(f"Unsupported device: {bus_name}")
                continue

            # Register devices on bus
            for device_item in bus_item.get('devices', []):

                device_address = device_item.get('address') or device_item.get('cs') or device_item.get('pin')
                device_class = device_item.get('class')
                device_name = device_item.get('name')
                if not device_name:
                    device_name = f"{device_class}_{device_address}"
                device_config = device_item.get("config")

                try:

                    device = create_device(device_address, device_class, device_name, device_config)
                    bus.register_device(device)

                    self.logger.info(
                        f"Registered class='{device_class}', '{device_name}' at {device_address} on {bus.name}")

                except Exception as e:
                    self.logger.error(f"Failed to register device: {e}")

            self.buses.append(bus)

    def start(self) -> None:
        """Start the emulator server"""
        self.load_config()

        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((self.host, self.port))
        self.socket.listen(5)

        self.running = True
        self.logger.info(f"Emulator listening on {self.host}:{self.port}")

        while self.running:
            try:
                client_socket, client_addr = self.socket.accept()
                thread = threading.Thread(target=self._handle_client,
                                          name=client_addr,
                                          args=(client_socket, client_addr,),
                                          daemon=True)
                thread.start()
            except Exception as e:
                if self.running:
                    self.logger.error(f"Accept error: {e}")

    def _handle_client(self, client_socket: socket.socket, client_addr) -> None:
        """ Handle connected client """
        try:

            self.logger.info(f"Client connected {client_addr}")

            while True:
                # Request header
                req_header = self._recv_exact(client_socket, REQ_HEADER_SIZE)
                if not req_header:
                    break

                magic, command, sequence, req_payload_len = struct.unpack(REQ_HEADER_FORMAT, req_header)
                if magic != PROTEUS_MAGIC:
                    raise Exception(f"Unexpected magic number: 0x{magic:08X}")

                # Request payload
                req_payload = b''
                if req_payload_len > 0:
                    req_payload = self._recv_exact(client_socket, req_payload_len)
                    if req_payload is None:
                        break

                # Handle command
                status, resp_payload = self._handle_command(command, req_payload)

                # Response header
                resp_header = struct.pack(RESP_HEADER_FORMAT, magic, status, sequence, len(resp_payload))

                # Send full response message
                client_socket.send(resp_header + resp_payload)

        except Exception as e:
            self.logger.error(f"Client handling error: {e}")
        finally:
            client_socket.close()
            self.logger.info(f"Client closed {client_addr}")

    def _find_by_type_and_id(self, target_type: DevType, target_id: int) -> Optional[BusBase]:
        for bus in self.buses:
            if bus.type == target_type and bus.id == target_id:
                return bus
        return None
    
    def _handle_command(self, command: int, req_payload: bytes) -> Tuple[ProteusStatus, bytes]:
        """Handle incoming client command and return status and result bytes"""

        status = ProteusStatus.SUCCESS
        resp_payload = b''

        try:
            if command == ProteusCommand.GET_DEVICES:
                # Get all devices                
                data_bytes = bytearray()

                # Count of devices
                data_bytes.extend(struct.pack(COUNT_OF_DEVICES_FORMAT, len(self.buses)))

                # Devices info
                for bus in self.buses:
                    data_bytes.extend(struct.pack(DEVICE_INFO_FORMAT,
                                                  bus.type, bus.name.encode('ascii')[:DEVICE_NAME_LEN]))                
                resp_payload = bytes(data_bytes)

            elif command == ProteusCommand.I2C_SET_SLAVE:
                # Set I2C slave address

                bus_id, slave_address = ProtocolMessage.decode_set_i2c_slave(req_payload)

                bus = self._find_by_type_and_id(DevType.I2C, bus_id)
                if bus:
                    bus.set_slave_address(slave_address)
                else:
                    status = ProteusStatus.DEVICE_NOT_FOUND

            elif command == ProteusCommand.I2C_TRANSACTION:
                # I2C transaction

                bus_id, messages = ProtocolMessage.decode_i2c_transaction(req_payload)

                bus = self._find_by_type_and_id(DevType.I2C, bus_id)
                if bus:
                    resp_payload = bus.transaction(messages)
                else:
                    status = ProteusStatus.DEVICE_NOT_FOUND

            elif command == ProteusCommand.SMBUS_TRANSACTION:
                # SMBus transaction

                bus_id, smbus_request = ProtocolMessage.decode_smbus_transaction(req_payload)

                bus = self._find_by_type_and_id(DevType.I2C, bus_id)
                if bus:
                    smbus_response = bus.smbus_transaction(smbus_request)
                    if smbus_response:
                        resp_payload = ProtocolMessage.encode_smbus_transaction(smbus_response)
                else:
                    status = ProteusStatus.DEVICE_NOT_FOUND

            else:
                self.logger.warning(f"Unsupported command: {command}")
                status = ProteusStatus.WRONG_INPUT

        except Exception as e:
            self.logger.error(f"Failed to handle command: {e}")
            status = ProteusStatus.COMMAND_FAILED

        return status, resp_payload

    def _recv_exact(self, sock: socket.socket, size: int) -> Optional[bytes]:
        """Receive exact number of bytes"""
        data = b''
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    def stop(self) -> None:
        """Stop the server emulator"""
        self.running = False
        if self.socket:
            self.socket.close()
        self.logger.info("Emulator stopped")
