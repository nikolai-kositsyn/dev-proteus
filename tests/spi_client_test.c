#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>


int main(int argc, char* argv[])
{
	const char* devName = "/dev/spidev0.0";

	if (argc > 1) devName = argv[1];	

	printf("=== SPI Client ===\n");
	printf("Device: %s\n\n", devName);

	// Open
	printf("[1] Opening device\n");
	int fd = open(devName, O_RDWR);
	if (fd < 0)
	{
		perror("    open failed");
		return 1;
	}
	printf("    Opened %s, fd=%d\n", devName, fd);

	// Settings
	printf("\n[2] Set/Get mode\n");

	uint8_t requestedMode = SPI_MODE_0; // SPI_MODE_0..SPI_MODE_3
	if (ioctl(fd, SPI_IOC_WR_MODE, &requestedMode) < 0)
	{
		perror("    ioctl SPI_IOC_WR_MODE failed");
		close(fd);
		return 1;
	}

	uint8_t actualMode = 0;	
	if (ioctl(fd, SPI_IOC_RD_MODE, &actualMode) < 0)
	{
		perror("    ioctl SPI_IOC_RD_MODE failed");
		close(fd);
		return 1;
	}

	if (requestedMode != actualMode)
	{
		printf("Requested mode=%u, but actual mode=%u\n", requestedMode, actualMode);
		close(fd);
		return 1;
	}

	printf("    Mode=%u\n", actualMode);

	printf("\n[3] Set/Get bits per word\n");

	uint8_t requestedBits = 8;	
	if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &requestedBits) < 0)
	{
		perror("    ioctl SPI_IOC_WR_BITS_PER_WORD failed");
		close(fd);
		return 1;
	}

	uint8_t actualBits = 0;	
	if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &actualBits) < 0)
	{
		perror("    ioctl SPI_IOC_RD_BITS_PER_WORD failed");
		close(fd);
		return 1;
	}

	if (requestedBits != actualBits)
	{
		printf("Requested bits=%u, but actual bits=%u\n", requestedBits, actualBits);
		close(fd);
		return 1;
	}

	printf("    BitsPerWord=%u\n", actualBits);

	printf("\n[4] Set/Get max speed hz\n");

	uint32_t requestedSpeed = 5000000;
	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &requestedSpeed) < 0)
	{
		perror("    ioctl SPI_IOC_WR_MAX_SPEED_HZ failed");
		close(fd);
		return 1;
	}

	uint32_t actualSpeed = 0;	
	if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &actualSpeed) < 0)
	{
		perror("    ioctl SPI_IOC_RD_MAX_SPEED_HZ failed");
		close(fd);
		return 1;
	}

	if (requestedSpeed != actualSpeed)
	{
		printf("Requested speed=%u, but actual speed=%u\n", requestedSpeed, actualSpeed);
		close(fd);
		return 1;
	}

	printf("    MaxSpeedHz=%u\n", actualSpeed);

	printf("\n[5] Set/Get LSB first\n");

	uint8_t requestedLSB = 1;	
	if (ioctl(fd, SPI_IOC_WR_LSB_FIRST, &requestedLSB) < 0)
	{
		perror("    ioctl SPI_IOC_WR_LSB_FIRST failed");
		close(fd);
		return 1;
	}

	uint8_t actualLSB = 0;	
	if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &actualLSB) < 0)
	{
		perror("    ioctl SPI_IOC_RD_LSB_FIRST failed");
		close(fd);
		return 1;
	}

	if (requestedLSB != actualLSB)
	{
		printf("Requested LSB=%u, but actual LSB=%u\n", requestedLSB, actualLSB);
		close(fd);
		return 1;
	}

	printf("    LSB First=%u\n", actualLSB);

	printf("\n[6] Set/Get mode32\n");

	uint32_t requestedMode32 = 1;
	if (ioctl(fd, SPI_IOC_WR_MODE32, &requestedMode32) < 0)
	{
		perror("    ioctl SPI_IOC_WR_MODE32 failed");
		close(fd);
		return 1;
	}

	uint32_t actualMode32 = 0;	
	if (ioctl(fd, SPI_IOC_RD_MODE32, &actualMode32) < 0)
	{
		perror("    ioctl SPI_IOC_RD_MODE32 failed");
		close(fd);
		return 1;
	}

	if (requestedMode32 != actualMode32)
	{
		printf("Requested Mode32=%u, but actual Mode32=%u\n", requestedMode32, actualMode32);
		close(fd);
		return 1;
	}

	printf("    Mode32=%u\n", actualMode32);

	// Write
	const uint8_t bufToWrite[] = { 0xAA, 0xBB, 0xCC, 0xDD };

	printf("\n[7] Writing %zu bytes\n", sizeof(bufToWrite));
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

	printf("\n[8] Reading %zu bytes\n", sizeof(bufToRead));
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

	printf("\n[9] Full Fuplex Transfer\n");
    uint8_t tx[] = { 0x01, 0x02, 0x03, 0x04 };
    uint8_t rx[sizeof(tx)] = { 0x00 };
    
    struct spi_ioc_transfer transfer = 
	{
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),        
    };
    
	if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0)
	{
		perror("    ioctl SPI_IOC_MESSAGE(1) failed");
		close(fd);
		return 1;
	}

	for (size_t idx = 0; idx < sizeof(tx); ++idx)
	{
		if (tx[idx] != rx[idx])
		{
			printf("    Full Duplex Transaction data mismatched at index %zu\n", idx);
			close(fd);
			return 1;
		}	
	}
	
	// Close
	printf("\n[9] Closing device\n");
	if (close(fd) < 0)
	{
		perror("    close failed");
		return 1;
	}
	printf("    Closed %s, fd=%d\n", devName, fd);

	printf("\n=== SPI Client Finished ===\n");
	return 0;
}