[![CI](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/ci.yml)
[![CodeQL](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/nikolai-kositsyn/dev-proteus/actions/workflows/codeql.yml)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![Python](https://img.shields.io/badge/python-3.7%2B-blue)](https://www.python.org/downloads/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20x86__64%20%7C%20arm%20%7C%20aarch64-blue)](https://www.linux.org/)


# dev-proteus - Universal Linux Peripheral Emulator

**dev-proteus** is a lightweight framework for emulating Linux peripheral devices using `LD_PRELOAD` technique. No kernel modules, no root privileges required.

> **⚠️ Status**: I2C (including SMBus) and SPI are fully supported. UART and GPIO support are planned for future releases.

## Features

- 🔌 **I2C/SMBus and SPI emulation** (fully working)
- 🔧 **LD_PRELOAD-based hooking** - intercepts system calls transparently
- 📝 **JSON configuration** - simple device and transaction definitions
- 🎯 **Transparent** - applications don't know they're talking to emulated devices
- 🐳 **Container-ready** - works perfectly in Docker environments
- 🚀 **No root required** for Python part (C hooks may need sudo for device access)

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