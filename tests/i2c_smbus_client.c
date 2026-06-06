#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>


// ========== SMBus wrappers (use I2C_SMBUS ioctl) ==========

static int ioctlErrorCounter = 0;

static inline int i2c_smbus_access(int fd, uint8_t read_write, uint8_t command,
	uint32_t size, union i2c_smbus_data* data)
{
	struct i2c_smbus_ioctl_data args = {
		.read_write = read_write,
		.command = command,
		.size = size,
		.data = data
	};

	int ioctlResult = ioctl(fd, I2C_SMBUS, &args);
	if (ioctlResult < 0)
	{
		++ioctlErrorCounter;
	}

	return ioctlResult;
}

int i2c_smbus_write_quick(int fd)
{
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, 0, I2C_SMBUS_QUICK, NULL);
}

int i2c_smbus_read_byte(int fd)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data))
		return -1;
	return data.byte & 0xFF;
}

int i2c_smbus_write_byte(int fd, uint8_t value)
{
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

int i2c_smbus_read_byte_data(int fd, uint8_t command)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(fd, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data))
		return -1;
	return data.byte & 0xFF;
}

int i2c_smbus_write_byte_data(int fd, uint8_t command, uint8_t value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
}

int i2c_smbus_read_word_data(int fd, uint8_t command)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(fd, I2C_SMBUS_READ, command, I2C_SMBUS_WORD_DATA, &data))
		return -1;
	return data.word & 0xFFFF;
}

int i2c_smbus_write_word_data(int fd, uint8_t command, uint16_t value)
{
	union i2c_smbus_data data;
	data.word = value;
	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA, &data);
}

int i2c_smbus_process_call(int fd, uint8_t command, uint16_t value)
{
	union i2c_smbus_data data;
	data.word = value;
	if (i2c_smbus_access(fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_PROC_CALL, &data))
		return -1;
	return data.word & 0xFFFF;
}

int i2c_smbus_read_block_data(int fd, uint8_t command, uint8_t* values)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(fd, I2C_SMBUS_READ, command, I2C_SMBUS_BLOCK_DATA, &data))
		return -1;

	int len = data.block[0];
	if (len > I2C_SMBUS_BLOCK_MAX)
		len = I2C_SMBUS_BLOCK_MAX;

	for (int i = 0; i < len; i++)
		values[i] = data.block[i + 1];

	return len;
}

int i2c_smbus_write_block_data(int fd, uint8_t command, uint8_t len, const uint8_t* values)
{
	if (len > I2C_SMBUS_BLOCK_MAX)
		len = I2C_SMBUS_BLOCK_MAX;

	union i2c_smbus_data data;
	data.block[0] = len;
	for (int i = 0; i < len; i++)
		data.block[i + 1] = values[i];

	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_BLOCK_DATA, &data);
}

int i2c_smbus_read_i2c_block_data(int fd, uint8_t command, uint8_t len, uint8_t* values)
{
	if (len > I2C_SMBUS_BLOCK_MAX)
		len = I2C_SMBUS_BLOCK_MAX;

	union i2c_smbus_data data;
	if (i2c_smbus_access(fd, I2C_SMBUS_READ, command, I2C_SMBUS_I2C_BLOCK_DATA, &data))
		return -1;

	int block_len = data.block[0];
	if (block_len > len)
		block_len = len;

	for (int i = 0; i < block_len; i++)
		values[i] = data.block[i + 1];

	return block_len;
}

int i2c_smbus_write_i2c_block_data(int fd, uint8_t command, uint8_t len, const uint8_t* values)
{
	if (len > I2C_SMBUS_BLOCK_MAX)
		len = I2C_SMBUS_BLOCK_MAX;

	union i2c_smbus_data data;
	data.block[0] = len;
	for (int i = 0; i < len; i++)
		data.block[i + 1] = values[i];

	return i2c_smbus_access(fd, I2C_SMBUS_WRITE, command, I2C_SMBUS_I2C_BLOCK_DATA, &data);
}

// ========== Dump helper ==========
void dump_bytes(const char* prefix, const uint8_t* data, int len)
{
	printf("%s", prefix);
	for (int i = 0; i < len; i++) {
		printf("%02X ", data[i]);
	}
	printf("\n");
}


int main(int argc, char* argv[])
{
	const char* devName = "/dev/i2c-3";
	int slaveAddr = 0x53;
	int result;

	if (argc > 1) devName = argv[1];
	if (argc > 2) slaveAddr = strtol(argv[2], NULL, 0);

	printf("=== SMBus Client ===\n");
	printf("Device: %s, Slave address: 0x%02X\n\n", devName, slaveAddr);

	// Open device
	int fd = open(devName, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	printf("[1] Opened %s, fd=%d\n", devName, fd);

	// Set address
	if (ioctl(fd, I2C_SLAVE, slaveAddr) < 0) {
		perror("ioctl I2C_SLAVE");
		close(fd);
		return 1;
	}
	printf("[2] Set slave address to 0x%02X\n", slaveAddr);

	// I2C_FUNCS
	printf("\n[3] Query I2C_FUNCS\n");
	unsigned long funcs = 0;
	if (ioctl(fd, I2C_FUNCS, &funcs) < 0) {
		perror("  I2C_FUNCS failed");
	}
	else {
		printf("  Supported functions: 0x%08lX\n", funcs);
		printf("    I2C_FUNC_SMBUS_QUICK:        %s\n",
			(funcs & I2C_FUNC_SMBUS_QUICK) ? "YES" : "NO");
		printf("    I2C_FUNC_SMBUS_BYTE:         %s\n",
			(funcs & I2C_FUNC_SMBUS_BYTE) ? "YES" : "NO");
		printf("    I2C_FUNC_SMBUS_BYTE_DATA:    %s\n",
			(funcs & I2C_FUNC_SMBUS_BYTE_DATA) ? "YES" : "NO");
		printf("    I2C_FUNC_SMBUS_WORD_DATA:    %s\n",
			(funcs & I2C_FUNC_SMBUS_WORD_DATA) ? "YES" : "NO");
		printf("    I2C_FUNC_SMBUS_BLOCK_DATA:   %s\n",
			(funcs & I2C_FUNC_SMBUS_BLOCK_DATA) ? "YES" : "NO");
		printf("    I2C_FUNC_SMBUS_I2C_BLOCK:    %s\n",
			(funcs & I2C_FUNC_SMBUS_I2C_BLOCK) ? "YES" : "NO");
	}

	// SMBus Quick Write
	printf("\n[4] SMBus Quick Write\n");
	result = i2c_smbus_write_quick(fd);
	if (result < 0)
		perror("  Quick Write failed");
	else
		printf("  Quick Write: OK\n");

	// SMBus Read Byte
	printf("\n[5] SMBus Read Byte\n");
	int byte_val = i2c_smbus_read_byte(fd);
	if (byte_val < 0)
		perror("  Read Byte failed");
	else
		printf("  Read Byte: 0x%02X (%d)\n", byte_val, byte_val);

	// SMBus Write Byte
	printf("\n[6] SMBus Write Byte (0xAB)\n");
	result = i2c_smbus_write_byte(fd, 0xAB);
	if (result < 0)
		perror("  Write Byte failed");
	else
		printf("  Write Byte: OK\n");

	// SMBus Read Byte Data
	printf("\n[7] SMBus Read Byte Data (command=0x10)\n");
	int reg_val = i2c_smbus_read_byte_data(fd, 0x10);
	if (reg_val < 0)
		perror("  Read Byte Data failed");
	else
		printf("  Read Byte Data: register 0x10 = 0x%02X\n", reg_val);

	// SMBus Write Byte Data
	printf("\n[8] SMBus Write Byte Data (command=0x10, value=0x55)\n");
	result = i2c_smbus_write_byte_data(fd, 0x10, 0x55);
	if (result < 0)
		perror("  Write Byte Data failed");
	else
		printf("  Write Byte Data: OK\n");

	// SMBus Read Word Data
	printf("\n[9] SMBus Read Word Data (command=0x20)\n");
	int word_val = i2c_smbus_read_word_data(fd, 0x20);
	if (word_val < 0)
		perror("  Read Word Data failed");
	else
		printf("  Read Word Data: register 0x20 = 0x%04X (%d)\n", word_val, word_val);

	// SMBus Write Word Data
	printf("\n[10] SMBus Write Word Data (command=0x20, value=0x1234)\n");
	result = i2c_smbus_write_word_data(fd, 0x20, 0x1234);
	if (result < 0)
		perror("  Write Word Data failed");
	else
		printf("  Write Word Data: OK\n");

	// SMBus Process Call
	printf("\n[11] SMBus Process Call (command=0x30, send=0xABCD)\n");
	int proc_result = i2c_smbus_process_call(fd, 0x30, 0xABCD);
	if (proc_result < 0)
		perror("  Process Call failed");
	else
		printf("  Process Call: received 0x%04X\n", proc_result);

	// SMBus Block Write
	printf("\n[12] SMBus Block Write (command=0x40)\n");
	uint8_t write_block[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
	dump_bytes("    Data: ", write_block, sizeof(write_block));
	result = i2c_smbus_write_block_data(fd, 0x40, sizeof(write_block), write_block);
	if (result < 0)
		perror("  Block Write failed");
	else
		printf("  Block Write: OK\n");

	// SMBus Block Read
	printf("\n[13] SMBus Block Read (command=0x40)\n");
	uint8_t read_block[I2C_SMBUS_BLOCK_MAX];
	int block_len = i2c_smbus_read_block_data(fd, 0x40, read_block);
	if (block_len < 0)
		perror("  Block Read failed");
	else {
		printf("  Block Read: %d bytes\n", block_len);
		dump_bytes("    Data: ", read_block, block_len);
	}

	// I2C Block Write
	printf("\n[14] I2C Block Write (command=0x50)\n");
	uint8_t i2c_write_block[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
	dump_bytes("    Data: ", i2c_write_block, sizeof(i2c_write_block));
	result = i2c_smbus_write_i2c_block_data(fd, 0x50, sizeof(i2c_write_block), i2c_write_block);
	if (result < 0)
		perror("  I2C Block Write failed");
	else
		printf("  I2C Block Write: OK\n");

	// I2C Block Read
	printf("\n[15] I2C Block Read (command=0x50, len=6)\n");
	uint8_t i2c_read_block[6] = { 0 };
	int i2c_block_len = i2c_smbus_read_i2c_block_data(fd, 0x50, sizeof(i2c_read_block), i2c_read_block);
	if (i2c_block_len < 0)
		perror("  I2C Block Read failed");
	else {
		printf("  I2C Block Read: %d bytes\n", i2c_block_len);
		dump_bytes("    Data: ", i2c_read_block, i2c_block_len);
	}

	// Close
	printf("\n[16] Closing device\n");
	if (close(fd) < 0) {
		perror("close");
		return 1;
	}
	printf("Closed %s\n", devName);

	printf("\n=== SMBus Client Finished (errors: %d) ===\n", ioctlErrorCounter);
	return ioctlErrorCounter;
}