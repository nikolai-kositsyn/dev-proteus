# dev-proteus - Clients Makefile

CC = gcc
CROSS_COMPILE ?=
TARGET_CC = $(CROSS_COMPILE)$(CC)

CFLAGS = -Wall -Wextra -O3 -std=c99
LDFLAGS = -ldl -lpthread

# Clients
I2C_CLIENT = i2c_client
I2C_EEPROM_CLIENT = i2c_eeprom_client
I2C_SMBUS_CLIENT = i2c_smbus_client

ALL_CLIENTS = $(I2C_CLIENT) $(I2C_EEPROM_CLIENT) $(I2C_SMBUS_CLIENT)

.PHONY: all clean

all: $(ALL_CLIENTS)
	@echo "Build complete. Target: $(if $(CROSS_COMPILE),$(CROSS_COMPILE),native)"

$(I2C_CLIENT): $(I2C_CLIENT).c
	$(TARGET_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(I2C_EEPROM_CLIENT): $(I2C_EEPROM_CLIENT).c
	$(TARGET_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(I2C_SMBUS_CLIENT): $(I2C_SMBUS_CLIENT).c
	$(TARGET_CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(ALL_CLIENTS)