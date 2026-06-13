# dev-proteus - Clients Makefile

CC = gcc
CROSS_COMPILE ?=
TARGET_CC = $(CROSS_COMPILE)$(CC)

# Default build type
BUILD_TYPE ?= release

# Common flags
COMMON_CFLAGS = -Wall -Wextra -std=c99
COMMON_LDFLAGS = -ldl -lpthread

# Debug flags (with ASan)
DEBUG_CFLAGS = -O0 -g -fsanitize=address -fno-omit-frame-pointer
DEBUG_LDFLAGS = -fsanitize=address

# Release flags
RELEASE_CFLAGS = -O3 -DNDEBUG
RELEASE_LDFLAGS =

# Select flags based on BUILD_TYPE
ifeq ($(BUILD_TYPE), debug)
    CFLAGS = $(COMMON_CFLAGS) $(DEBUG_CFLAGS)
    LDFLAGS = $(COMMON_LDFLAGS) $(DEBUG_LDFLAGS)
else
    CFLAGS = $(COMMON_CFLAGS) $(RELEASE_CFLAGS)
    LDFLAGS = $(COMMON_LDFLAGS) $(RELEASE_LDFLAGS)
endif

# I2C Clients
I2C_CLIENT = i2c_client_test
I2C_EEPROM_CLIENT = i2c_eeprom_client_test
I2C_SMBUS_CLIENT = i2c_smbus_client_test
I2C_MULTI_CLIENT = i2c_multi_client_test

# SPI Clients
SPI_CLIENT = spi_client_test
SPI_MULTI_CLIENT = spi_multi_client_test

ALL_CLIENTS = $(I2C_CLIENT) \
              $(I2C_EEPROM_CLIENT) \
              $(I2C_SMBUS_CLIENT) \
              $(I2C_MULTI_CLIENT) \
			  $(SPI_CLIENT) \
			  $(SPI_MULTI_CLIENT)

.PHONY: all clean debug release help

# Default target
all: $(ALL_CLIENTS)
	@echo "Build complete. Target: $(if $(CROSS_COMPILE),$(CROSS_COMPILE),native)"
	@echo "Build type: $(BUILD_TYPE)"

# Build with debug
debug: clean
	$(MAKE) BUILD_TYPE=debug all

# Build with release
release: clean
	$(MAKE) BUILD_TYPE=release all

# Compile and link
%: %.c
	$(TARGET_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(ALL_CLIENTS)

help:
	@echo "dev-proteus Clients Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build clients (default, release mode)"
	@echo "  debug    - Build with AddressSanitizer and debug symbols"
	@echo "  release  - Build with optimizations (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE    - Build type (debug or release)"
	@echo "  CROSS_COMPILE - Cross-compilation prefix"
	@echo ""
	@echo "Examples:"
	@echo "  make                      # Release build"
	@echo "  make debug                # Debug build with ASan"
	@echo "  make CROSS_COMPILE=aarch64-linux-gnu-  # Cross-compile for ARM64"
	@echo "  make debug CROSS_COMPILE=arm-linux-gnueabihf-  # Debug for ARM32"