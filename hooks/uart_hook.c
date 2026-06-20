/**
 * dev-proteus - UART Functionality
 * Handles UART device operations (/dev/ttyS*, /dev/ttyUSB*, /dev/ttyAMA*, ...)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>

#include "common.h"
#include "proteus.h"

//=============================================================================
// UART private types
//=============================================================================

typedef struct UartDeviceS
{
    // Common part (fd, name, id, clientSock, methods)
    VirtualDevice base;    
} UartDevice;

//=============================================================================
// FNV-1A calculation
//=============================================================================

static uint32_t calc_fnv1a(const char *name)
{
    const uint32_t FNV_OFFSET_BASIS = 0x811c9dc5u;
    const uint32_t FNV_PRIME = 0x01000193u;

    uint32_t hash = FNV_OFFSET_BASIS;
    size_t len = strlen(name);

    for (size_t i = 0; i < len; i++) 
    {
        hash ^= (uint8_t)name[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

//=============================================================================
// UART server communication functions
//=============================================================================

// Helper: send request and receive response
static int uart_run_command(VirtualDevice* dev, uint16_t cmd,
                            const uint8_t* reqPayload, uint16_t reqPayloadLen,
                            uint8_t* respPayload, uint16_t respBufMaxLen, uint16_t* respPayloadLen)
{
    uint16_t sequence = proteus_next_sequence();
    if (proteus_send_request(dev->clientSock, cmd, sequence, reqPayload, reqPayloadLen) < 0)
    {
        return -1;
    }

    return proteus_recv_response(dev->clientSock, sequence, respPayload, respBufMaxLen, respPayloadLen);
}

//-----------------------------------------------------------------------------
// Setup UART parameters (terminal)
//-----------------------------------------------------------------------------

static int set_uart_termios(VirtualDevice* dev, const struct termios* term)
{
    uint8_t reqPayload[sizeof(dev->id) + sizeof(struct termios)];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &dev->id, sizeof(dev->id));
    reqPayloadLen += sizeof(dev->id);

    memcpy(reqPayload + reqPayloadLen, term, sizeof(struct termios));
    reqPayloadLen += sizeof(struct termios);

    uint16_t respLen = 0;
    return uart_run_command(dev, PROTEUS_CMD_UART_SET_TERMIOS,
                reqPayload, reqPayloadLen, NULL, 0, &respLen);
}

static int get_uart_termios(VirtualDevice* dev, struct termios* term)
{
    uint8_t reqPayload[sizeof(dev->id)];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &dev->id, sizeof(dev->id));
    reqPayloadLen += sizeof(dev->id);

    uint8_t respPayload[sizeof(struct termios)];
    uint16_t respLen = 0;

    int rc = uart_run_command(dev, PROTEUS_CMD_UART_GET_TERMIOS,
                               reqPayload, reqPayloadLen, respPayload, sizeof(respPayload), &respLen);

    if (rc == 0 && respLen == sizeof(struct termios))
    {
        memcpy(term, respPayload, sizeof(struct termios));
        return 0;
    }

    return -1;
}

//-----------------------------------------------------------------------------
// Lines control (modem signals)
//-----------------------------------------------------------------------------

static int set_uart_modem_status(VirtualDevice* dev, int status)
{
    uint8_t reqPayload[sizeof(dev->id) + sizeof(status)];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &dev->id, sizeof(dev->id));
    reqPayloadLen += sizeof(dev->id);

    memcpy(reqPayload + reqPayloadLen, &status, sizeof(status));
    reqPayloadLen += sizeof(status);

    uint16_t respPayloadLen = 0;
    return uart_run_command(dev, PROTEUS_CMD_UART_SET_MODEM,
                             reqPayload, reqPayloadLen, NULL, 0, &respPayloadLen);
}

static int get_uart_modem_status(VirtualDevice* dev, int* status)
{
    uint8_t reqPayload[sizeof(dev->id)];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &dev->id, sizeof(dev->id));
    reqPayloadLen += sizeof(dev->id);

    uint8_t respPayload[sizeof(*status)];
    uint16_t respPayloadLen = 0;

    int rc = uart_run_command(dev, PROTEUS_CMD_UART_GET_MODEM,
                               reqPayload, reqPayloadLen, respPayload, sizeof(respPayload), &respPayloadLen);

    if (rc == 0 && respPayloadLen == sizeof(*status))
    {
        memcpy(status, respPayload, sizeof(*status));
        return 0;
    }

    return -1;
}

static int get_count_of_available_bytes(VirtualDevice* dev, uint32_t* countOfBytes)
{
    uint8_t reqPayload[sizeof(dev->id)];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &dev->id, sizeof(dev->id));
    reqPayloadLen += sizeof(dev->id);

    uint8_t respPayload[sizeof(*countOfBytes)];
    uint16_t respPayloadLen = 0;

    int rc = uart_run_command(dev, PROTEUS_CMD_UART_GET_AVAILABLE_BYTES,
                               reqPayload, reqPayloadLen, respPayload, sizeof(respPayload), &respPayloadLen);

    if (rc == 0 && respPayloadLen == sizeof(*countOfBytes))
    {
        memcpy(countOfBytes, respPayload, sizeof(*countOfBytes));
        return 0;
    }

    return -1;
}

//=============================================================================
// Forward declarations for UART implementations
//=============================================================================

static int uart_hook_close(VirtualDevice* device);
static ssize_t uart_hook_read(VirtualDevice* device, void* buf, size_t len);
static ssize_t uart_hook_write(VirtualDevice* device, const void* buf, size_t count);
static int uart_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp);

//=============================================================================
// UART 'open' implementation
//=============================================================================

void* uart_hook_open(const char* name, int flags, ...)
{
    (void)flags;

    PROTEUS_LOG("[UART] %s: open, flags=%X", name, flags);

    UartDevice* device = NULL;    

    int fd = g_proteusCtx.real_open("/dev/null", O_RDWR);
    if (fd > 0)
    {
        device = (UartDevice*)malloc(sizeof(UartDevice));
        if (device != NULL)
        {
            int clientSock = proteus_connect();
            if (clientSock >= 0)
            {
                // Common
                device->base.fd = fd;
                device->base.type = DEV_TYPE_UART_E;

                strncpy(device->base.name, name, sizeof(device->base.name) - 1);
                
                // Unique id is calculated hash from full name
                device->base.id = calc_fnv1a(name);
                device->base.clientSock = clientSock;

                device->base.impl_close = uart_hook_close;
                device->base.impl_read  = uart_hook_read;
                device->base.impl_write = uart_hook_write;
                device->base.impl_ioctl = uart_hook_ioctl;

                device->base.next = NULL;

                // UART specific                            
            }
            else
            {
                g_proteusCtx.real_close(fd);

                free(device);
                device = NULL;
            }
        }
        else
        {
            PROTEUS_LOG("Failed to malloc when create UART device");
            g_proteusCtx.real_close(fd);
        }
    }
    else
    {
        PROTEUS_LOG("Failed to open /dev/null when create UART device (%d)", fd);
    }

    return (void*)device;
}

//=============================================================================
// UART 'close', 'read', 'write', 'ioctl'
//=============================================================================

static int uart_hook_close(VirtualDevice* device)
{
    PROTEUS_LOG("[UART] %s: close, fd=%d", device->name, device->fd);

    int closeResult = g_proteusCtx.real_close(device->fd);
    proteus_disconnect(device->clientSock);
    free((void*)device);

    return closeResult;
}

static ssize_t uart_hook_read(VirtualDevice* device, void* buf, size_t len)
{
    PROTEUS_LOG("[UART] %s: read, fd=%d, len=%zu", device->name, device->fd, len);    
    
    uint16_t bytesToRead = (uint16_t)len;

    uint8_t reqPayload[sizeof(device->id) + sizeof(bytesToRead)];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &device->id, sizeof(device->id));
    reqPayloadLen += sizeof(device->id);

    memcpy(reqPayload + reqPayloadLen, &bytesToRead, sizeof(bytesToRead));
    reqPayloadLen += sizeof(bytesToRead);

    uint8_t respPayload[PROTEUS_PAYLOAD_MAX_SIZE];
    uint16_t respPayloadLen = 0;

    int rc = uart_run_command(device, PROTEUS_CMD_UART_READ, reqPayload, reqPayloadLen, 
                                respPayload, sizeof(respPayload), &respPayloadLen);

    if (rc == 0 && respPayloadLen > 0)
    {
        uint16_t bytesToCopy = (respPayloadLen < bytesToRead) ? respPayloadLen : bytesToRead;
        memcpy(buf, respPayload, bytesToCopy);

        return (ssize_t)bytesToCopy;
    }

    return rc;
}

static ssize_t uart_hook_write(VirtualDevice* device, const void* buf, size_t count)
{
    PROTEUS_LOG("[UART] %s: write, fd=%d, count=%zu", device->name, device->fd, count);
    
    uint16_t bytesToWrite = (uint16_t)count;

    if (sizeof(device->id) + bytesToWrite > PROTEUS_PAYLOAD_MAX_SIZE)
    {
        errno = EFBIG;
        return -1;
    }

    uint8_t reqPayload[PROTEUS_PAYLOAD_MAX_SIZE];
    uint16_t reqPayloadLen = 0;

    memcpy(reqPayload + reqPayloadLen, &device->id, sizeof(device->id));
    reqPayloadLen += sizeof(device->id);    

    memcpy(reqPayload + reqPayloadLen, buf, bytesToWrite);
    reqPayloadLen += bytesToWrite;

    uint16_t respPayloadLen = 0;
    int rc = uart_run_command(device, PROTEUS_CMD_UART_WRITE,
                               reqPayload, reqPayloadLen, NULL, 0, &respPayloadLen);
    if (rc == 0)
    {
        return (ssize_t)bytesToWrite;
    }

    return rc;
}

static int uart_hook_ioctl(VirtualDevice* device, unsigned long request, void* argp)
{
    PROTEUS_LOG("[UART] %s: ioctl, fd=%d, request=0x%08lX", device->name, device->fd, request);

    int rc = 0;

    switch (request)
    {
        // Get termios
        case TCGETS:
        {
            struct termios* term = (struct termios*)argp;
            rc = get_uart_termios(device, term);
            break;
        }

        // Set termios
        case TCSANOW: // Optional action of 'tcsetattr'
        case TCSADRAIN: // Optional action of 'tcsetattr'
        case TCSAFLUSH: // Optional action of 'tcsetattr'
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
        {
            const struct termios* term = (const struct termios*)argp;
            rc = set_uart_termios(device, term);
            break;
        }

        // Get modem lines
        case TIOCMGET:
        {
            int* status = (int*)argp;
            rc = get_uart_modem_status(device, status);
            break;
        }

        // Set modem lines
        case TIOCMSET:
        case TIOCMBIC:
        case TIOCMBIS:
        {
            const int* status = (const int*)argp;
            rc = set_uart_modem_status(device, *status);
            break;
        }
        
        case FIONREAD: // Get the number of bytes in the input buffer
        //case TIOCINQ: // Same as FIONREAD
        {
            uint32_t* countOfBytes = (uint32_t*)argp;
            rc = get_count_of_available_bytes(device, countOfBytes);
            break;
        }

        case TIOCOUTQ: // Get the number of bytes in the output buffer        
        {            
            uint32_t* countOfBytes = (uint32_t*)argp;
            *countOfBytes = 0;
            rc = 0;
            break;
        }

        default:
        {
            PROTEUS_LOG("[UART] %s: Unsupported ioctl request: 0x%08lX", device->name, request);            
            errno = ENOTTY;
            rc = -1;

            break;
        }
    }

    return rc;
}