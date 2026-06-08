# dev-proteus - Hook Library Makefile

CC = gcc
CROSS_COMPILE ?=
TARGET_CC = $(CROSS_COMPILE)$(CC)

# Default build type
BUILD_TYPE ?= release

# Common flags
COMMON_CFLAGS = -Wall -Wextra -fPIC -std=c99 -D_GNU_SOURCE
COMMON_LDFLAGS = -shared -ldl -lpthread

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

# Source files
PROTEUS_SRC = proteus.c
HOOK_MANAGER_SRC = hook_manager.c
I2C_HOOK_SRC = i2c_hook.c
SPI_HOOK_SRC = spi_hook.c
UART_HOOK_SRC = uart_hook.c

ALL_SRC = $(PROTEUS_SRC) $(HOOK_MANAGER_SRC) $(I2C_HOOK_SRC) $(SPI_HOOK_SRC) $(UART_HOOK_SRC)
ALL_OBJ = $(ALL_SRC:.c=.o)

TARGET_LIB = libproteus_hook.so

.PHONY: all clean debug release help

# Default target
all: $(TARGET_LIB)

# Build with debug
debug: clean
	$(MAKE) BUILD_TYPE=debug all

# Build with release
release: clean
	$(MAKE) BUILD_TYPE=release all

# Compile
%.o: %.c common.h proteus.h
	$(TARGET_CC) $(CFLAGS) -c -o $@ $<

# Link library
$(TARGET_LIB): $(ALL_OBJ)
	$(TARGET_CC) $(LDFLAGS) -o $@ $^

# Clean
clean:
	rm -f $(ALL_OBJ) $(TARGET_LIB)

# Help
help:
	@echo "dev-proteus Hook Library Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build library (default, release mode)"
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
	@echo "  make                 # Release build"
	@echo "  make debug           # Debug build with ASan"
	@echo "  make CROSS_COMPILE=aarch64-linux-gnu-  # Cross-compile for ARM64"
	@echo "  make debug CROSS_COMPILE=arm-linux-gnueabihf-  # Debug for ARM32"