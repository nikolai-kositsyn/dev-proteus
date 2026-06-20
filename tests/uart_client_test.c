#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
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

void print_modem_status(int status)
{
    printf("        RTS=%d, CTS=%d, DTR=%d, DSR=%d, DCD=%d, RI=%d\n",
           (status & TIOCM_RTS) ? 1 : 0,
           (status & TIOCM_CTS) ? 1 : 0,
           (status & TIOCM_DTR) ? 1 : 0,
           (status & TIOCM_DSR) ? 1 : 0,
           (status & TIOCM_CAR) ? 1 : 0,
           (status & TIOCM_RNG) ? 1 : 0);
}

void print_termios_settings(struct termios *tty)
{
    printf("    Speed: ");
    speed_t ispeed = cfgetispeed(tty);
    speed_t ospeed = cfgetospeed(tty);
    printf("in=%d, out=%d\n", ispeed, ospeed);
    
    printf("    Data bits: ");
    switch (tty->c_cflag & CSIZE) {
        case CS5: printf("5\n"); break;
        case CS6: printf("6\n"); break;
        case CS7: printf("7\n"); break;
        case CS8: printf("8\n"); break;
        default: printf("unknown\n");
    }
    
    printf("    Parity: ");
    if (tty->c_cflag & PARENB) {
        if (tty->c_cflag & PARODD)
            printf("odd\n");
        else
            printf("even\n");
    } else {
        printf("none\n");
    }
    
    printf("    Stop bits: %d\n", (tty->c_cflag & CSTOPB) ? 2 : 1);
    printf("    Flow control: %s\n", (tty->c_cflag & CRTSCTS) ? "hardware" : "none/software");
    printf("    Canonical mode: %s\n", (tty->c_lflag & ICANON) ? "enabled" : "disabled");
    printf("    Echo: %s\n", (tty->c_lflag & ECHO) ? "enabled" : "disabled");
}

int main(int argc, char* argv[])
{
#ifdef _FORTIFY_SOURCE
    printf("_FORTIFY_SOURCE = %d\n", _FORTIFY_SOURCE);
#else
    printf("_FORTIFY_SOURCE is not defined\n");
#endif

    const char* devName = "/dev/ttyS0";
    int baudrate = B115200;
    
    if (argc > 1) devName = argv[1];
    if (argc > 2) baudrate = atoi(argv[2]);
    
    printf("=== UART Simple Client ===\n");
    printf("Device: %s, Baudrate: %d\n\n", devName, baudrate);
    
    // 1. Open
    printf("[1] Opening device\n");
    int fd = open(devName, O_RDWR);
    if (fd < 0) {
        perror("    open failed");
        return 1;
    }
    printf("    Opened %s, fd=%d\n", devName, fd);
    
    // 2. isatty - check if device is a terminal
    printf("\n[2] Checking if device is a TTY\n");
    int is_tty = isatty(fd);
    printf("    isatty(fd) = %d %s\n", is_tty, is_tty ? "(is a TTY)" : "(is NOT a TTY)");
    
    // 3. tcgetattr - get current settings
    printf("\n[3] Getting current terminal attributes (tcgetattr)\n");
    struct termios old_tty, new_tty;
    if (tcgetattr(fd, &old_tty) < 0) {
        perror("    tcgetattr failed");
        close(fd);
        return 1;
    }
    printf("    Current settings:\n");
    print_termios_settings(&old_tty);
    
    // 4. tcsetattr - set new settings (raw mode)
    printf("\n[4] Setting new terminal attributes (tcsetattr) - raw mode\n");
    new_tty = old_tty;
    
    // Set speed
    cfsetispeed(&new_tty, baudrate);
    cfsetospeed(&new_tty, baudrate);
    
    // Configure: 8N1, no hardware flow control
    new_tty.c_cflag &= ~CSIZE;
    new_tty.c_cflag |= CS8;
    new_tty.c_cflag &= ~PARENB;
    new_tty.c_cflag &= ~CSTOPB;
    new_tty.c_cflag &= ~CRTSCTS;
    new_tty.c_cflag |= CREAD | CLOCAL;
    
    // Raw mode: minimal processing
    new_tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);
    new_tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    new_tty.c_oflag &= ~OPOST;
    
    // Timeout settings
    new_tty.c_cc[VMIN] = 1;
    new_tty.c_cc[VTIME] = 0;
    
    if (tcsetattr(fd, TCSANOW, &new_tty) < 0) {
        perror("    tcsetattr failed");
        close(fd);
        return 1;
    }
    printf("    New settings applied:\n");
    print_termios_settings(&new_tty);
    
    // 5. ioctl TIOCMGET - get modem lines status
    printf("\n[5] Getting modem lines status (TIOCMGET)\n");
    int modem_status;
    if (ioctl(fd, TIOCMGET, &modem_status) < 0) {
        perror("    ioctl TIOCMGET failed");
        close(fd);
        return 1;
    }
    printf("    Modem status (0x%04X):\n", modem_status);
    print_modem_status(modem_status);
    
    // 6. ioctl TIOCMSET - set modem lines status
    printf("\n[6] Setting modem lines (TIOCMSET) - enabling DTR and RTS\n");
    int new_modem_status = modem_status | TIOCM_DTR | TIOCM_RTS;
    if (ioctl(fd, TIOCMSET, &new_modem_status) < 0) {
        perror("    ioctl TIOCMSET failed");
        close(fd);
        return 1;
    }
    printf("    New modem status (0x%04X):\n", new_modem_status);
    print_modem_status(new_modem_status);
    
    // 7. ioctl TIOCMBIS - set bits
    printf("\n[7] Setting modem bits (TIOCMBIS) - enabling DSR and DCD\n");
    int set_bits = TIOCM_DSR | TIOCM_CAR;
    if (ioctl(fd, TIOCMBIS, &set_bits) < 0) {
        perror("    ioctl TIOCMBIS failed");
        close(fd);
        return 1;
    }
    // Check new status
    if (ioctl(fd, TIOCMGET, &modem_status) < 0) {
        perror("    ioctl TIOCMGET failed");
        close(fd);
        return 1;
    }
    printf("    Modem status after TIOCMBIS (0x%04X):\n", modem_status);
    print_modem_status(modem_status);
    
    // 8. ioctl TIOCMBIC - clear bits
    printf("\n[8] Clearing modem bits (TIOCMBIC) - disabling DSR and DCD\n");
    int clear_bits = TIOCM_DSR | TIOCM_CAR;
    if (ioctl(fd, TIOCMBIC, &clear_bits) < 0) {
        perror("    ioctl TIOCMBIC failed");
        close(fd);
        return 1;
    }
    // Check new status
    if (ioctl(fd, TIOCMGET, &modem_status) < 0) {
        perror("    ioctl TIOCMGET failed");
        close(fd);
        return 1;
    }
    printf("    Modem status after TIOCMBIC (0x%04X):\n", modem_status);
    print_modem_status(modem_status);
    
    // 9. Write - send data
    const uint8_t bufToWrite[] = "Hello UART!";
    size_t write_len = strlen((char*)bufToWrite);
    printf("\n[9] Writing %zu bytes\n", write_len);
    ssize_t writtenBytes = write(fd, bufToWrite, write_len);
    if (writtenBytes < 0) {
        perror("    write failed");
        close(fd);
        return 1;
    }
    printf("    Written %zd bytes: \"%s\"\n", writtenBytes, bufToWrite);    

    // 10. ioctl FIONREAD - get available bytes for reading (after write, before read)
    printf("\n[10] Checking available bytes for read (FIONREAD)\n");
    int bytes_available = 0;
    if (ioctl(fd, FIONREAD, &bytes_available) < 0) {
        perror("    ioctl FIONREAD failed");
        close(fd);
        return 1;
    }
    printf("    Bytes available to read: %d\n", bytes_available);
    
    // 11. Read - read exactly bytes_available bytes
    printf("\n[11] Reading %d bytes\n", bytes_available);
    uint8_t bufToRead[256] = {0};
    
    if (bytes_available > 0) {
        printf("Attempt to read from %d %d bytes\n", fd, bytes_available);

        ssize_t readBytes = read(fd, bufToRead, bytes_available);
        if (readBytes < 0) {
            perror("    read failed");
            close(fd);
            return 1;
        }
        printf("    Read %zd bytes: \"%s\"\n", readBytes, bufToRead);
    } else {
        printf("    No data available to read\n");
    }
    
    // 12. Restore original terminal settings
    printf("\n[12] Restoring original terminal settings\n");
    if (tcsetattr(fd, TCSANOW, &old_tty) < 0) {
        perror("    tcsetattr restore failed");
    } else {
        printf("    Original settings restored\n");
    }
    
    // 13. Close
    printf("\n[13] Closing device\n");
    if (close(fd) < 0) {
        perror("    close failed");
        return 1;
    }
    printf("    Closed %s, fd=%d\n", devName, fd);
    
    printf("\n=== UART Simple Client Finished ===\n");
    return 0;
}