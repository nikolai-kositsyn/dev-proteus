#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>


int main(int argc, char* argv[])
{
	const char* devName = "/dev/i2c-2";
	int slaveAddr = 0x52;

	if (argc > 1) devName = argv[1];
	if (argc > 2) slaveAddr = strtol(argv[2], NULL, 0);

	printf("=== I2C EEPROM Client ===\n");
	printf("Device: %s, Slave address: 0x%02X\n\n", devName, slaveAddr);

	// Open
	printf("[1] Opening device\n");
	int fd = open(devName, O_RDWR);
	if (fd < 0)
	{
		perror("    open failed");
		return 1;
	}
	printf("    Opened %s, fd=%d\n", devName, fd);

	uint8_t eepromAddress[] = { 0x01, 0x02 };
	uint8_t eepromWriteData[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
	uint8_t eepromReadData[sizeof(eepromWriteData)] = { 0 };

	// Write
	printf("\n[2] Writing content by address 0x%02X%02X\n", eepromAddress[0], eepromAddress[1]);

	uint8_t eepromTotalWrite[sizeof(eepromAddress) + sizeof(eepromWriteData)] = { 0 };
	memcpy(eepromTotalWrite, eepromAddress, sizeof(eepromAddress));
	memcpy(eepromTotalWrite + sizeof(eepromAddress), eepromWriteData, sizeof(eepromWriteData));

	struct i2c_msg writeMsgs[] = {
		{
			.addr = slaveAddr, .flags = 0, .len = sizeof(eepromTotalWrite), .buf = eepromTotalWrite
		},
	};

	struct i2c_rdwr_ioctl_data writeTransaction = {
		.msgs = writeMsgs, .nmsgs = sizeof(writeMsgs) / sizeof(struct i2c_msg),
	};

	int ioctlWriteResult = ioctl(fd, I2C_RDWR, &writeTransaction);
	if (ioctlWriteResult < 0)
	{
		perror("    ioctl I2C_RDWR failed");
		close(fd);
		return 1;
	}

	printf("    Written %zu bytes: ", sizeof(eepromWriteData));
	for (size_t idx = 0; idx < sizeof(eepromWriteData); ++idx)
	{
		printf("0x%02X ", eepromWriteData[idx]);
	}
	printf("\n");

	printf("\n[3] Reading content by address 0x%02X%02X\n", eepromAddress[0], eepromAddress[1]);
	struct i2c_msg readMsgs[] = {
		{
			.addr = slaveAddr, .flags = 0, .len = sizeof(eepromAddress), .buf = eepromAddress
		},

		{
			.addr = slaveAddr, .flags = I2C_M_RD, .len = sizeof(eepromReadData), .buf = eepromReadData
		},
	};

	struct i2c_rdwr_ioctl_data readTransaction = {
		.msgs = readMsgs, .nmsgs = sizeof(readMsgs) / sizeof(struct i2c_msg),
	};

	int ioctlReadResult = ioctl(fd, I2C_RDWR, &readTransaction);
	if (ioctlReadResult < 0)
	{
		perror("    ioctl I2C_RDWR failed");
		close(fd);
		return 1;
	}

	printf("    Read %zu bytes: ", sizeof(eepromReadData));
	for (size_t idx = 0; idx < sizeof(eepromReadData); ++idx)
	{
		printf("0x%02X ", eepromReadData[idx]);
	}
	printf("\n");

#define MIN(a,b) (((a)<(b))?(a):(b))
	for (size_t idx = 0; idx < MIN(sizeof(eepromWriteData), sizeof(eepromReadData)); ++idx)
	{
		if (eepromWriteData[idx] != eepromReadData[idx])
		{
			printf("    Read/Written data mismatched at index %zu\n", idx);
			close(fd);
			return 1;
		}
	}

	// Close
	printf("\n[4] Closing device\n");
	if (close(fd) < 0)
	{
		perror("    close failed");
		return 1;
	}
	printf("    Closed %s, fd=%d\n", devName, fd);

	printf("\n=== I2C EEPROM Client Finished ===\n");
	return 0;
}