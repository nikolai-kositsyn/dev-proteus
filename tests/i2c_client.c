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
	const char* devName = "/dev/i2c-1";
	int slaveAddr = 0x51;

	if (argc > 1) devName = argv[1];
	if (argc > 2) slaveAddr = strtol(argv[2], NULL, 0);

	printf("=== I2C Simple Client ===\n");
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

	// Set Slave Address
	printf("\n[2] Setting slave address to 0x%02X\n", slaveAddr);
	if (ioctl(fd, I2C_SLAVE, slaveAddr) < 0)
	{
		perror("    ioctl I2C_SLAVE failed");
		close(fd);
		return 1;
	}
	printf("    Set slave addr=0x%02X\n", slaveAddr);

	// Set Slave Address Force
	printf("\n[3] Setting slave address (force) to 0x%02X\n", slaveAddr);
	if (ioctl(fd, I2C_SLAVE_FORCE, slaveAddr) < 0)
	{
		perror("    ioctl I2C_SLAVE_FORCE failed");
		close(fd);
		return 1;
	}
	printf("    Set slave addr=0x%02X (force)\n", slaveAddr);

	// Write
	const uint8_t bufToWrite[] = { 0xAA, 0xBB, 0xCC, 0xDD };

	printf("\n[4] Writing %zu bytes\n", sizeof(bufToWrite));
	ssize_t writtenBytes = write(fd, bufToWrite, sizeof(bufToWrite));
	if (writtenBytes != sizeof(bufToWrite))
	{
		perror("    write failed");
		close(fd);
		return 1;
	}
	printf("    Written %zu bytes: ", writtenBytes);
	for (ssize_t idx = 0; idx < writtenBytes; ++idx)
	{
		printf("0x%02X ", bufToWrite[idx]);
	}
	printf("\n");

	// Read
	uint8_t bufToRead[sizeof(uint32_t)] = { 0 };

	printf("\n[5] Reading %zu bytes\n", sizeof(bufToRead));
	ssize_t readBytes = read(fd, bufToRead, sizeof(bufToRead));
	if (readBytes != sizeof(bufToRead))
	{
		perror("    read failed");
		close(fd);
		return 1;
	}
	printf("    Read %zd bytes: ", readBytes);
	for (ssize_t idx = 0; idx < readBytes; ++idx)
	{
		printf("0x%02X ", bufToRead[idx]);
	}
	printf("\n");

	// Close
	printf("\n[6] Closing device\n");
	if (close(fd) < 0)
	{
		perror("    close failed");
		return 1;
	}
	printf("    Closed %s, fd=%d\n", devName, fd);

	printf("\n=== I2C Simple Client Finished ===\n");
	return 0;
}