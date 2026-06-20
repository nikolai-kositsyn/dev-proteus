#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

// Handle different flow control definitions
#ifndef CRTSCTS
#ifdef CNEW_RTSCTS
#define CRTSCTS CNEW_RTSCTS
#else
#define CRTSCTS 0
#endif
#endif

#define NUM_CLIENTS 3
#define TEST_DATA_SIZE 32

typedef struct {
    int client_id;
    char dev_name[32];
    int baudrate;
    uint32_t unique_id;
} client_data_t;

uint32_t generate_unique_id(int client_id) {
    return (uint32_t)(client_id * 0x9E3779B9) ^ 0xDEADBEEF;
}

int configure_uart(int fd, int baudrate) {
    struct termios tty;
    
    if (tcgetattr(fd, &tty) < 0) {
        return -1;
    }
    
    // Set speed
    cfsetispeed(&tty, baudrate);
    cfsetospeed(&tty, baudrate);
    
    // Configure: 8N1, no hardware flow control
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    
    // Raw mode: minimal processing
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    tty.c_oflag &= ~OPOST;
    
    // Timeout settings
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;
    
    if (tcsetattr(fd, TCSANOW, &tty) < 0) {
        return -1;
    }
    
    return 0;
}

void* client_thread(void* arg) {
    client_data_t* data = (client_data_t*)arg;
    uint8_t write_buffer[TEST_DATA_SIZE];
    uint8_t read_buffer[TEST_DATA_SIZE];
    int bytes_available = 0;
    
    // Prepare write buffer with unique ID
    memset(write_buffer, 0, TEST_DATA_SIZE);
    write_buffer[0] = (data->unique_id >> 0) & 0xFF;
    write_buffer[1] = (data->unique_id >> 8) & 0xFF;
    write_buffer[2] = (data->unique_id >> 16) & 0xFF;
    write_buffer[3] = (data->unique_id >> 24) & 0xFF;
    // Fill rest with pattern
    for (int i = 4; i < TEST_DATA_SIZE; i++) {
        write_buffer[i] = (uint8_t)(i + data->client_id);
    }
    
    printf("[Client %d] Starting on %s, ID=0x%08X\n",
           data->client_id, data->dev_name, data->unique_id);
    
    // Open device
    int fd = open(data->dev_name, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("[Client %d] Failed to open %s: %s\n", 
               data->client_id, data->dev_name, strerror(errno));
        return (void*)-1;
    }
    printf("[Client %d] Opened %s, fd=%d\n", data->client_id, data->dev_name, fd);
    
    // Check if device is a TTY
    if (!isatty(fd)) {
        printf("[Client %d] Warning: %s is not a TTY\n", data->client_id, data->dev_name);
    }
    
    // Configure UART
    if (configure_uart(fd, data->baudrate) < 0) {
        printf("[Client %d] Failed to configure UART: %s\n", 
               data->client_id, strerror(errno));
        close(fd);
        return (void*)-1;
    }
    printf("[Client %d] Configured UART: 8N1, %d baud\n", 
           data->client_id, data->baudrate);
    
    // Write unique ID to echo device
    printf("[Client %d] Writing %d bytes...\n", data->client_id, TEST_DATA_SIZE);
    ssize_t writtenBytes = write(fd, write_buffer, TEST_DATA_SIZE);
    if (writtenBytes < 0) {
        printf("[Client %d] Write failed: %s\n", data->client_id, strerror(errno));
        close(fd);
        return (void*)-1;
    }
    printf("[Client %d] Wrote %zd bytes, ID: 0x%08X\n", 
           data->client_id, writtenBytes, data->unique_id);
    
    // Check available bytes for reading (FIONREAD)
    if (ioctl(fd, FIONREAD, &bytes_available) < 0) {
        printf("[Client %d] FIONREAD failed: %s\n", data->client_id, strerror(errno));
        close(fd);
        return (void*)-1;
    }
    printf("[Client %d] Bytes available to read: %d\n", data->client_id, bytes_available);
    
    // Read back from echo device
    if (bytes_available > 0) {
        // Limit read to TEST_DATA_SIZE
        int bytes_to_read = bytes_available < TEST_DATA_SIZE ? bytes_available : TEST_DATA_SIZE;
        ssize_t readBytes = read(fd, read_buffer, bytes_to_read);
        if (readBytes < 0) {
            printf("[Client %d] Read failed: %s\n", data->client_id, strerror(errno));
            close(fd);
            return (void*)-1;
        }
        printf("[Client %d] Read %zd bytes\n", data->client_id, readBytes);
        
        // Verify data
        uint32_t read_id = (uint32_t)read_buffer[0] |
                          ((uint32_t)read_buffer[1] << 8) |
                          ((uint32_t)read_buffer[2] << 16) |
                          ((uint32_t)read_buffer[3] << 24);
        
        printf("[Client %d] Read ID: 0x%08X\n", data->client_id, read_id);
        
        // Compare
        if (data->unique_id == read_id) {
            printf("[Client %d] SUCCESS - Data verified!\n", data->client_id);
            close(fd);
            return (void*)0;
        } else {
            printf("[Client %d] FAILED - Data mismatch! Expected 0x%08X, got 0x%08X\n",
                   data->client_id, data->unique_id, read_id);
            // Print hex dump for debugging
            printf("[Client %d] Read buffer: ", data->client_id);
            for (int i = 0; i < readBytes && i < 16; i++) {
                printf("%02X ", read_buffer[i]);
            }
            printf("\n");
            close(fd);
            return (void*)-1;
        }
    } else {
        printf("[Client %d] No data available to read (echo may not be enabled)\n", 
               data->client_id);
        close(fd);
        return (void*)-1;
    }
}

int main() 
{
    const char* serial_devices[NUM_CLIENTS] = {
        "/dev/ttyS0", "/dev/ttyUSB1", "/dev/ttyAMA1",
    };

    pthread_t threads[NUM_CLIENTS];
    client_data_t clients_data[NUM_CLIENTS];
    int success_count = 0;
    int fail_count = 0;
    
    printf("===========================================================\n");
    printf("UART Multi-Threaded Echo Test\n");
    printf("Using UART echo devices (echo mode)\n");
    printf("===========================================================\n\n");
    
    #ifdef _FORTIFY_SOURCE
    printf("_FORTIFY_SOURCE = %d\n", _FORTIFY_SOURCE);
#else
    printf("_FORTIFY_SOURCE is not defined\n");
#endif

    // Create client data and generate unique IDs
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients_data[i].client_id = i + 1;
        snprintf(clients_data[i].dev_name, sizeof(clients_data[i].dev_name), "%s", serial_devices[i]);        
        clients_data[i].baudrate = B115200;
        clients_data[i].unique_id = generate_unique_id(i + 1);
    }
    
    // Create and start all threads
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (pthread_create(&threads[i], NULL, client_thread, &clients_data[i]) != 0) {
            printf("Failed to create thread for client %d\n", i + 1);
            return 1;
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_CLIENTS; i++) {
        void* retval;
        pthread_join(threads[i], &retval);
        if (retval == 0) {
            success_count++;
        } else {
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
        printf("All tests passed! Hook library is thread-safe.\n");
    } else {
        printf("Some tests failed! Thread-safety issues detected.\n");
    }
    
    return (fail_count > 0) ? 1 : 0;
}