"""Main emulator server"""

import socket
import threading
from typing import Dict, Optional
import struct

from dev_proteus.core.protocol import *
from dev_proteus.buses.bus_base import BusBase
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
        self.buses: Dict[str, BusBase] = {}
        self.socket: Optional[socket.socket] = None
        self.running = False
        self.logger = get_logger()

        self.sequence_counter = 0
        self._sequence_lock = threading.Lock()

    def _next_sequence(self) -> int:
        """Get next sequence number"""
        with self._sequence_lock:
            self.sequence_counter += 1
            return self.sequence_counter

    def load_config(self) -> None:
        """Load configuration from JSON file"""
        if not self.config_path:
            self.logger.warning("No config file provided")
            return

        config = load_config(self.config_path)

        for bus_item in config.get('buses', []):
            bus_type = bus_item.get('type')
            bus_id = bus_item.get('bus_id')

            if bus_type == 'i2c':
                bus = I2CBus(bus_id)
            elif bus_type == 'spi':
                bus = SPIBus(bus_id, bus_item.get('config', {}))
            elif bus_type == 'uart':
                bus = UARTBus(bus_item.get('port', 'ttyS0'), bus_item.get('config', {}))
            elif bus_type == 'gpio':
                bus = GPIOBus(bus_item.get('chip', 0))
            else:
                self.logger.warning(f"Unknown bus type: {bus_type}")
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
                        f"Registered class='{device_class}', '{device_name}' at {device_address} on {bus_type} bus {bus_id}")

                except Exception as e:
                    self.logger.error(f"Failed to register device: {e}")

            self.buses[bus.name] = bus

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
        """Handle connected client"""
        try:

            self.logger.info(f"Client connected {client_addr}")

            while True:
                # Read message header               

                header_data = self._recv_exact(client_socket, HEADER_SIZE)
                if not header_data:
                    break

                magic, msg_type, sequence, payload_len = struct.unpack(HEADER_FORMAT, header_data)

                # Read message payload
                payload = b''
                if payload_len > 0:
                    payload = self._recv_exact(client_socket, payload_len)
                    if payload is None:
                        break

                # Process message
                response_data = self._process_message(msg_type, sequence, payload)

                # Send response
                client_socket.send(response_data)

        except Exception as e:
            self.logger.error(f"Client handling error: {e}")
        finally:
            client_socket.close()
            self.logger.info(f"Client closed {client_addr}")

    def _process_message(self, msg_type: int, sequence: int, payload: bytes) -> bytes:
        """Process incoming message and return response"""
        try:
            if msg_type == ProteusMessageType.I2C_SET_SLAVE:
                bus_id, slave_address = ProtocolMessage.decode_set_i2c_slave(payload)

                # Handle set I2C slave address
                status = ProteusStatus.SUCCESS

                bus = self.buses.get(f"i2c-{bus_id}")
                if bus:
                    bus.set_slave_address(slave_address)
                else:
                    status = ProteusStatus.DEVICE_NOT_FOUND

                return struct.pack(FOOTER_FORMAT, status, 0)

            elif msg_type == ProteusMessageType.I2C_TRANSACTION:
                bus_id, messages = ProtocolMessage.decode_i2c_transaction(payload)

                # Handle I2C transaction
                status = ProteusStatus.SUCCESS
                read_data = b''

                bus = self.buses.get(f"i2c-{bus_id}")
                if bus:
                    read_data = bus.transaction(messages)
                else:
                    status = ProteusStatus.DEVICE_NOT_FOUND

                return struct.pack(FOOTER_FORMAT, status, 0) + read_data

            elif msg_type == ProteusMessageType.SMBUS_TRANSACTION:
                bus_id, smbus_request = ProtocolMessage.decode_smbus_transaction(payload)

                # Handle SMBus transaction
                status = ProteusStatus.SUCCESS
                response_bytes = b''

                bus = self.buses.get(f"i2c-{bus_id}")
                if bus:
                    smbus_response = bus.smbus_transaction(smbus_request)
                    if smbus_response:
                        response_bytes = ProtocolMessage.encode_smbus_transaction(smbus_response)
                else:
                    status = ProteusStatus.DEVICE_NOT_FOUND

                # Footer with status and optional response bytes
                return struct.pack(FOOTER_FORMAT, status, 0) + response_bytes

            else:
                self.logger.warning(f"Unsupported message: {msg_type}")
                return struct.pack(FOOTER_FORMAT, ProteusStatus.WRONG_INPUT, 0)

        except Exception as e:
            self.logger.error(f"Error processing message: {e}")
            return struct.pack(FOOTER_FORMAT, ProteusStatus.COMMAND_FAILED, 0)

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
