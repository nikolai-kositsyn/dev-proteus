[![CI](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/ci.yml)
[![CodeQL](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/codeql.yml)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![Python](https://img.shields.io/badge/python-3.7%2B-blue)](https://www.python.org/downloads/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20x86__64%20%7C%20arm%20%7C%20aarch64-blue)](https://www.linux.org/)


# dev-proteus - Universal Linux Peripheral Emulator

**dev-proteus** is a lightweight framework for emulating Linux peripheral devices using `LD_PRELOAD` technique. No kernel modules, no root privileges required.

## Supported Protocols and Interfaces

### ✅ I2C (Inter-Integrated Circuit)

Full-featured I2C bus emulation with support for both legacy and modern Linux I2C interfaces.

#### Core Operations
- **Standard read/write** — Simple byte-level I2C transfers via `read()` and `write()` syscalls
- **Modern ioctl-based transfers** — Full support for `I2C_RDWR` with multi-message transactions (atomic combined write/read sequences)
- **SMBus operations** — Complete SMBus protocol support via `I2C_SMBUS` ioctl, including:
  - Quick command (`I2C_SMBUS_QUICK`)
  - Byte (`I2C_SMBUS_BYTE`)
  - Byte data (`I2C_SMBUS_BYTE_DATA`)
  - Word data (`I2C_SMBUS_WORD_DATA`)
  - Block data (`I2C_SMBUS_BLOCK_DATA`)
  - I2C block data (`I2C_SMBUS_I2C_BLOCK_DATA`)
  - Process call (`I2C_SMBUS_PROC_CALL`)

#### Slave Address Management
- `I2C_SLAVE` — Set slave address with device checking
- `I2C_SLAVE_FORCE` — Set slave address without checking
- `I2C_TENBIT` — Enable/disable 10-bit addressing

#### Capabilities
- `I2C_FUNCS` — Reports supported functionality (`I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL`)
- Thread-safe concurrent access to multiple buses


### ✅ SPI (Serial Peripheral Interface)

Full-featured SPI bus emulation with comprehensive `ioctl`-based control interface.

#### Core Operations
- **Full-duplex communication** — Simultaneous transmit and receive via `SPI_IOC_MESSAGE`
- **Standard read/write** — Simple transfers via `read()` and `write()` syscalls (half-duplex)
- **Multi-message transactions** — Atomic transfer of multiple SPI messages in a single `ioctl` call (`SPI_IOC_MESSAGE(n)`)

#### Configuration Interface (`ioctl`-based)
Full support for all standard SPI device attributes:
- **SPI Mode** — `SPI_IOC_RD_MODE` / `SPI_IOC_WR_MODE` (CPOL, CPHA configuration)
- **SPI Mode32** — `SPI_IOC_RD_MODE32` / `SPI_IOC_WR_MODE32` (extended 32-bit mode flags)
- **Bits Per Word** — `SPI_IOC_RD_BITS_PER_WORD` / `SPI_IOC_WR_BITS_PER_WORD` (8, 16, 32 bits, etc.)
- **Maximum Speed** — `SPI_IOC_RD_MAX_SPEED_HZ` / `SPI_IOC_WR_MAX_SPEED_HZ` (clock frequency in Hz)
- **LSB First** — `SPI_IOC_RD_LSB_FIRST` / `SPI_IOC_WR_LSB_FIRST` (bit order: LSB or MSB first)

#### Transfer Features
- **Per-message configuration** — Each transfer in a multi-message transaction can have its own:
  - Speed (`speed_hz`)
  - Bits per word (`bits_per_word`)
  - Delay after transfer (`delay_usecs`)
  - Word delay (`word_delay_usecs`)
  - Chip select change behavior (`cs_change`)
  - TX/RX data width (`tx_nbits`, `rx_nbits`)

#### Capabilities
- Full-duplex and half-duplex operation modes
- Thread-safe concurrent access to multiple buses


### ✅ UART / TTY (Universal Asynchronous Receiver-Transmitter)

Full-featured serial port emulation with comprehensive TTY and modem control.

#### Core Operations
- **Standard TTY operations** — `open()` / `close()` / `read()` / `write()` / `ioctl()`
- **FORTIFY_SOURCE support** — Transparent handling of fortified calls (`__read_chk`, `__write_chk`)
- **Multi-client support** — Thread-safe concurrent access to multiple UART devices

#### Terminal Configuration (`termios` via `ioctl`)
Complete `termios` interface for serial port configuration:
- **Get/Set attributes** — `TCGETS` / `TCSETS` / `TCSETSW` / `TCSETSF`
- **Line discipline** — Raw and canonical modes
- **Baud rate** — Independent input/output speed configuration
- **Data format** — Data bits (5-8), parity (none/even/odd), stop bits (1/2)
- **Flow control** — Hardware (RTS/CTS) and software (XON/XOFF)
- **Control characters** — VINTR, VQUIT, VERASE, VKILL, VEOF, etc.
- **Echo and signal handling** — ISIG, ICANON, ECHO, IEXTEN

#### Modem Line Control
Full modem signal management via `ioctl`:
- **Get line status** — `TIOCMGET` (RTS, CTS, DTR, DSR, DCD, RI)
- **Set line status** — `TIOCMSET` (replace all)
- **Set individual bits** — `TIOCMBIS` (set selected bits)
- **Clear individual bits** — `TIOCMBIC` (clear selected bits)

#### Buffer Management
- **Input buffer query** — `FIONREAD` (get available bytes for reading)
- **Output buffer query** — `TIOCOUTQ` (get bytes waiting in output buffer)

#### Supported Device Nodes
- **Legacy serial ports** — `/dev/ttyS*`
- **USB serial adapters** — `/dev/ttyUSB*`
- **ARM serial ports** — `/dev/ttyAMA*`
- **Any TTY device** — All `/dev/tty*` devices

#### Capabilities
- Full termios compatibility
- Modem signal emulation
- Thread-safe concurrent access to multiple ports


### 🚧 GPIO (planned)
- Pin state control
- Interrupt emulation
- Edge detection

## Features
- 🔌 **Multi-interface support** — I2C (including SMBus), SPI, and UART (TTY) emulation
- 🔧 **LD_PRELOAD-based hooking** — intercepts system calls transparently
- 📝 **JSON configuration** — simple device and transaction definitions
- 🧵 **Thread-safe** — supports multi-client concurrent access
- 🎯 **Transparent** — applications don't know they're talking to emulated devices
- 🐳 **Container-ready** — works perfectly in Docker environments
- 🚀 **No root required** for Python part (C hooks may need sudo for device access)
- 🔒 **FORTIFY_SOURCE compatible** — handles fortified glibc calls (`__read_chk`, `__write_chk`)

## Quick Start

### Build the hook library and test clients
```bash
chmod +x ./scripts/*.sh
./scripts/build_hooks.sh
./scripts/build_clients.sh
```
This creates libproteus_hook.so and test clients in output/ directory relative to project root

### Terminal A: Start emulator with debug logging on test configuration
```bash
python3 src/dev_proteus/cli.py --log-level d -c configs/test_config.json
```

### Terminal B (Option 1): Run any client with LD_PRELOAD explicitly
```bash
cd ./output

LD_PRELOAD=./libproteus_hook.so ./i2c_client_test
LD_PRELOAD=./libproteus_hook.so ./i2c_eeprom_client_test
LD_PRELOAD=./libproteus_hook.so ./i2c_smbus_client_test
LD_PRELOAD=./libproteus_hook.so ./i2c_multi_client_test

LD_PRELOAD=./libproteus_hook.so ./spi_client_test
LD_PRELOAD=./libproteus_hook.so ./spi_multi_client_test

LD_PRELOAD=./libproteus_hook.so ./uart_client_test
LD_PRELOAD=./libproteus_hook.so ./uart_multi_client_test
```

### Terminal B (Option 2): Run all test clients
```bash
./scripts/run_tests.sh
```

## Configuration Example
```json
{
  "buses": [
    {
      "name": "/dev/i2c-1",
      "devices": [
        {
          "address": "0x51",
          "class": "i2c-echo",
          "name": "Echo Device"
        }
      ]
    },
    
    {
      "name": "/dev/i2c-2",
      "devices": [
        {
          "address": "0x52",
          "class": "i2c-eeprom",
          "name": "24LC256"
        }
      ]
    },
    
    {
      "name": "/dev/i2c-3",
      "devices": [
        {
          "address": "0x53",
          "class": "i2c-smbus",
          "name": "SMBus Device"
        }
      ]
    },
    
    {
      "name": "/dev/i2c-4",
      "devices": [
        {
          "address": "0x54",
          "class": "i2c-custom-flow",
          "name": "Custom Flow Device",
          "config": {
            "flow": [
              {
                "request": "7E 000000000000 0000 04 01C1 00 FFFF 00 0000 000C 00010300 0008 0000",
                "response": "7E 000000000000 0000 00 0241 00 FFFF 00 0000 0010 00010300 0008 0064 C010232000000000",
                "delay": 10
              }
            ]
          }
        }
      ]
    },
    
    {
      "name": "/dev/spidev0.0",
      "config": {
        "mode": 0,
        "bits_per_word": 8,
        "max_speed_hz": 1000000,
        "lsb_first": 0
      },
      "devices": [
        {
          "cs": "0",
          "class": "spi-echo",
          "name": "SPI Echo Device"
        }
      ]
    },
    
    {
      "name": "/dev/spidev1.1",
      "config": {
        "mode": 0,
        "bits_per_word": 8,
        "max_speed_hz": 1000000,
        "lsb_first": 0
      },
      "devices": [
        {
          "cs": "0",
          "class": "spi-echo",
          "name": "SPI Echo Device"
        },
        {
          "cs": "1",
          "class": "spi-echo",
          "name": "SPI Echo Device"
        }
      ]
    },

    {
      "name": "/dev/ttyS0",
      "config": {
        "bitrate": 115200,
        "data_bits": 8,
        "parity": "none",
        "stop_bits": 1,
        "flow_control": "none"
      },
      "devices": [
        {
          "address": "0x00",
          "class": "uart-echo",
          "name": "UART Echo Device"
        }
      ]
    },

    {
      "name": "/dev/ttyUSB1",
      "config": {
        "bitrate": 115200,
        "data_bits": 8,
        "parity": "none",
        "stop_bits": 1,
        "flow_control": "none"
      },
      "devices": [
        {
          "address": "0x00",
          "class": "uart-echo",
          "name": "UART Echo Device"
        }
      ]
    }

  ]
}
```

## Hook Library and Test Clients Build Options
### Build with defaults (127.0.0.1:4242, timeout=500ms, retry=3, delay=100ms)
```bash
./scripts/build_all.sh
```

### Build with custom host and port
```bash
PROTEUS_HOST=192.168.0.80 PROTEUS_PORT=9000 ./scripts/build_all.sh
```

### Build with custom timeout, retry and delay
```bash
PROTEUS_TIMEOUT_MS=100 PROTEUS_RETRY=5 PROTEUS_DELAY_MS=10 ./scripts/build_all.sh
```

### Build for ARM64
```bash
CROSS_COMPILE=aarch64-linux-gnu- ./scripts/build_all.sh
```

### Disable verbose logging
```bash
PROTEUS_VERBOSE=0 ./scripts/build_all.sh
```

### Debug build with ASan

**AddressSanitizer (ASan)** is integrated for detecting memory leaks, buffer overflows, and use-after-free errors.

```bash
# Build with ASan
PROTEUS_BUILD_TYPE=debug ./scripts/build_all.sh

# Run tests with ASan
PROTEUS_BUILD_TYPE=debug ./scripts/run_tests.sh
```