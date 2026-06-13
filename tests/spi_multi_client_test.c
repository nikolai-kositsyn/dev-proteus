#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define NUM_CLIENTS		(2)
#define SPI_BUS_START	(0)
#define SPI_CS_START	(0)

typedef struct 
{
	int client_id;
	char dev_name[32];
	uint32_t unique_id;
} client_data_t;


uint32_t generate_unique_id(int client_id) {
	return (uint32_t)(client_id * 0x9E3779B9) ^ 0xDEADBEEF;
}

void* client_thread(void* arg) {
	client_data_t* data = (client_data_t*)arg;
	uint8_t write_buffer[sizeof(uint32_t)];
	uint8_t read_buffer[sizeof(uint32_t)];

	write_buffer[0] = (data->unique_id >> 0) & 0xFF;
	write_buffer[1] = (data->unique_id >> 8) & 0xFF;
	write_buffer[2] = (data->unique_id >> 16) & 0xFF;
	write_buffer[3] = (data->unique_id >> 24) & 0xFF;

	printf("[Client %d] Starting on %s, ID=0x%08X\n",
		data->client_id, data->dev_name, data->unique_id);

	// Open device
	int fd = open(data->dev_name, O_RDWR);
	if (fd < 0) {
		printf("[Client %d] ❌ Failed to open %s\n", data->client_id, data->dev_name);
		return (void*)-1;
	}
	printf("[Client %d] Opened %s, fd=%d\n", data->client_id, data->dev_name, fd);

	// Write unique ID to echo device
	struct spi_ioc_transfer write_trans = 
	{
		.tx_buf = (unsigned long)write_buffer,
        .rx_buf = (unsigned long)0,
        .len = sizeof(write_buffer),
	};

	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &write_trans);
	if (ret < 0) {
		printf("[Client %d] ❌ Write failed!\n", data->client_id);
		close(fd);
		return (void*)-1;
	}
	printf("[Client %d] Wrote ID: 0x%08X\n", data->client_id, data->unique_id);

	// Read back from echo device
	struct spi_ioc_transfer read_trans = 
	{
		.tx_buf = (unsigned long)0,
        .rx_buf = (unsigned long)read_buffer,
        .len = sizeof(read_buffer),
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &read_trans);
	if (ret < 0) {
		printf("[Client %d] ❌ Read failed!\n", data->client_id);
		close(fd);
		return (void*)-1;
	}

	uint32_t read_id = (uint32_t)read_buffer[0] |
		((uint32_t)read_buffer[1] << 8) |
		((uint32_t)read_buffer[2] << 16) |
		((uint32_t)read_buffer[3] << 24);

	printf("[Client %d] Read  ID: 0x%08X\n", data->client_id, read_id);

	close(fd);

	// Verify data
	if (data->unique_id == read_id) {
		printf("[Client %d] ✅ SUCCESS - Data verified!\n", data->client_id);
		return (void*)0;
	}
	else {
		printf("[Client %d] ❌ FAILED - Data mismatch! Expected 0x%08X, got 0x%08X\n",
			data->client_id, data->unique_id, read_id);
		return (void*)-1;
	}
}

int main() {
	pthread_t threads[NUM_CLIENTS];
	client_data_t clients_data[NUM_CLIENTS];
	int success_count = 0;
	int fail_count = 0;

	printf("===========================================================\n");
	printf("SPI Multi-Threaded Echo Test\n");	
	printf("Using spi-echo devices (no address, just echo)\n");
	printf("===========================================================\n\n");

	// Create client data and generate unique IDs
	for (int i = 0; i < NUM_CLIENTS; i++) 
	{
		clients_data[i].client_id = i + 1;
		snprintf(clients_data[i].dev_name, sizeof(clients_data[i].dev_name),
			"/dev/spidev%d.%d", SPI_BUS_START + i, SPI_CS_START + i);
		clients_data[i].unique_id = generate_unique_id(i + 1);
	}

	// Create and start all threads
	for (int i = 0; i < NUM_CLIENTS; i++) {
		if (pthread_create(&threads[i], NULL, client_thread, &clients_data[i]) != 0) {
			printf("❌ Failed to create thread for client %d\n", i + 1);
			return 1;
		}
	}

	// Wait for all threads to complete
	for (int i = 0; i < NUM_CLIENTS; i++) {
		void* retval;
		pthread_join(threads[i], &retval);
		if (retval == 0) {
			success_count++;
		}
		else {
			fail_count++;
		}
	}

	printf("\n===========================================================\n");
	printf("TEST SUMMARY\n");
	printf("===========================================================\n");
	printf("Total clients:   %d\n", NUM_CLIENTS);
	printf("Successful:      %d\n", success_count);
	printf("Failed:          %d\n", fail_count);
	printf("Success rate:    %.1f%%\n", (success_count * 100.0f) / NUM_CLIENTS);
	printf("===========================================================\n");

	if (fail_count == 0) {
		printf("✅ All tests passed! Hook library is thread-safe.\n");
	}
	else {
		printf("❌ Some tests failed! Thread-safety issues detected.\n");
	}

	return (fail_count > 0) ? 1 : 0;
}