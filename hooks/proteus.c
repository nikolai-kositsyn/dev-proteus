#include "proteus.h"

#include <string.h>
#include <unistd.h>     // Required for close(), read(), and write()
#include <dlfcn.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <stdatomic.h>

#include "common.h"

//=============================================================================
// Internal data
//=============================================================================

static pthread_mutex_t s_dns_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint s_sequence = 0;

//=============================================================================
// Init / Destroy
//=============================================================================

void proteus_init() {
}

void proteus_destroy()
{
	pthread_mutex_destroy(&s_dns_mutex);
}

//=============================================================================
// Socket communication
//=============================================================================

static int proteus_resolve_host(const char* host, struct sockaddr_in* addr)
{
	int resolveResult = -1;
	if (!host || !addr)
	{
		return resolveResult;
	}

	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(g_proteus_config.emulator_port);

	pthread_mutex_lock(&s_dns_mutex);

	if (inet_pton(AF_INET, host, &addr->sin_addr) == 1)
	{
		resolveResult = 0;
	}
	else
	{
		struct hostent* he = gethostbyname(host);
		if (he && he->h_addr_list && he->h_addr_list[0])
		{
			memcpy(&addr->sin_addr, he->h_addr_list[0], he->h_length);
			resolveResult = 0;
		}
		else
		{
			PROTEUS_LOG("Failed to resolve host: %s", host);
		}
	}

	pthread_mutex_unlock(&s_dns_mutex);
	return resolveResult;
}

uint32_t proteus_next_sequence()
{
	return atomic_fetch_add(&s_sequence, 1) + 1;
}

int proteus_connect()
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		PROTEUS_LOG("Failed to create socket: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in hostAddr;
	if (proteus_resolve_host(g_proteus_config.emulator_host, &hostAddr) < 0)
	{
		close(sock);
		return -1;
	}

	// Set socket timeout
	struct timeval timeout;
	timeout.tv_sec = PROTEUS_TIMEOUT_MS / 1000;
	timeout.tv_usec = (PROTEUS_TIMEOUT_MS % 1000) * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	// Attempt connection with retries
	int retry = PROTEUS_RETRY_COUNT;
	while (retry-- > 0)
	{
		if (connect(sock, (struct sockaddr*)&hostAddr, sizeof(hostAddr)) == 0)
		{
			PROTEUS_LOG("Connected to emulator at %s:%d",
				g_proteus_config.emulator_host,
				g_proteus_config.emulator_port);

			return sock;
		}

		usleep(100000); // 100ms
	}

	PROTEUS_LOG("Failed to connect to %s:%d, timeout: %d ms, retry: %d (%s)",
		g_proteus_config.emulator_host, g_proteus_config.emulator_port,
		PROTEUS_TIMEOUT_MS, PROTEUS_RETRY_COUNT, strerror(errno));
	close(sock);

	return -1;
}

void proteus_disconnect(int clientSock)
{
	if (clientSock >= 0)
	{
		int closeResult = close(clientSock);
		PROTEUS_LOG("Disconnected from emulator: %s (%d)",
			strerror(closeResult), closeResult);
	}
}

int proteus_send_transaction(int clientSock, uint32_t msg_type, uint32_t sequence,
	const uint8_t* payload, uint32_t payload_len)
{
	ProteusHeader header;
	header.magic = PROTEUS_MAGIC;
	header.msg_type = msg_type;
	header.sequence = sequence;
	header.payload_len = payload_len;

	// Send header
	ssize_t sent = send(clientSock, &header, sizeof(header), 0);
	if (sent != sizeof(header))
	{
		PROTEUS_LOG("Failed to send header: %s", strerror(errno));
		return -1;
	}

	// Send payload
	if (payload_len > 0 && payload)
	{
		sent = send(clientSock, payload, payload_len, 0);
		if (sent != (ssize_t)payload_len)
		{
			PROTEUS_LOG("Failed to send payload: %s", strerror(errno));
			return -1;
		}
	}

	PROTEUS_LOG(">> msg=%u, seq=%u, payload=%u bytes",
		msg_type, sequence, payload_len);
	return 0;
}

int proteus_recv_response(int clientSock, uint8_t* respPayload, const uint32_t expectedLen)
{
	// Receive footer first
	ProteusFooter footer;
	ssize_t recvd = recv(clientSock, &footer, sizeof(footer), 0);

	if (recvd != sizeof(footer))
	{
		PROTEUS_LOG("Failed to receive footer: %s", strerror(errno));
		return -1;
	}

	const char* statusStr = NULL;
	switch (footer.status)
	{
	case PROTEUS_STATUS_SUCCESS:
	{
		statusStr = "SUCCESS";
	}
	break;

	case PROTEUS_STATUS_DEVICE_NOT_FOUND:
	{
		statusStr = "DEVICE_NOT_FOUND";
		errno = ENODEV;
	}
	break;

	case PROTEUS_STATUS_WRONG_INPUT:
	{
		statusStr = "WRONG_INPUT";
		errno = EINVAL;
	}
	break;

	case PROTEUS_STATUS_COMMAND_FAILED:
	{
		statusStr = "COMMAND_FAILED";
		errno = EFAULT;
	}
	break;

	case PROTEUS_STATUS_TIMEOUT:
	{
		statusStr = "TIMEOUT";
		errno = ETIMEDOUT;
	}
	break;

	default:
	{
		statusStr = "UNKNOWN";
	}
	break;
	}

	(void)statusStr;
	PROTEUS_LOG("<< %s (%d)", statusStr, footer.status);

	if (footer.status != PROTEUS_STATUS_SUCCESS)
	{
		return -1;
	}

	// Try to receive 'read data' if expected
	if (expectedLen > 0)
	{
		if (!respPayload)
		{
			PROTEUS_LOG("Failed to receive read data: respPayload is NULL but expectedLen=%u", expectedLen);
			return -1;
		}

		uint32_t receivedBytes = 0;
		while (receivedBytes < expectedLen)
		{
			recvd = recv(clientSock,
				respPayload + receivedBytes,
				expectedLen - receivedBytes, 0);
			if (recvd <= 0)
			{
				PROTEUS_LOG("Failed to receive read data: %s", strerror(errno));
				return -1;
			}

			receivedBytes += recvd;
		}

		PROTEUS_LOG("<< data=%u bytes", receivedBytes);
	}

	return 0;
}
