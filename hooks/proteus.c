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
static atomic_ushort s_sequence = 0;

//=============================================================================
// Init / Destroy
//=============================================================================

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
	addr->sin_port = htons(g_proteusCtx.emulatorPort);

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

uint16_t proteus_next_sequence()
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
	if (proteus_resolve_host(g_proteusCtx.emulatorHost, &hostAddr) < 0)
	{
		g_proteusCtx.real_close(sock);
		return -1;
	}

	// Set socket timeout
	struct timeval timeout;
	timeout.tv_sec = g_proteusCtx.emulatorTimeoutMs / 1000;
	timeout.tv_usec = (g_proteusCtx.emulatorTimeoutMs % 1000) * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	// Attempt connection with retries
	int retry = g_proteusCtx.emulatorRetry;
	while (retry-- > 0)
	{
		if (connect(sock, (struct sockaddr*)&hostAddr, sizeof(hostAddr)) == 0)
		{
			PROTEUS_LOG("Connected to emulator at %s:%d",
				g_proteusCtx.emulatorHost, g_proteusCtx.emulatorPort);

			return sock;
		}

		usleep(g_proteusCtx.emulatorDelayMs * 1000);
	}

	PROTEUS_LOG("Failed to connect to %s:%d, timeout: %d ms, retry: %d (%s)",
		g_proteusCtx.emulatorHost, g_proteusCtx.emulatorPort,
		g_proteusCtx.emulatorTimeoutMs, g_proteusCtx.emulatorRetry, strerror(errno));
	
	g_proteusCtx.real_close(sock);

	return -1;
}

void proteus_disconnect(int clientSock)
{
	if (clientSock >= 0)
	{
		int closeResult = g_proteusCtx.real_close(clientSock);
		PROTEUS_LOG("Disconnected from emulator: %s (%d)",
			strerror(closeResult), closeResult);
	}
}

int proteus_send_request(int clientSock, uint16_t command, uint16_t sequence,
	const uint8_t* payload, uint16_t payloadLen)
{
	ProteusReqHeader reqHeader;
	reqHeader.magic = PROTEUS_MAGIC;
	reqHeader.command = command;
	reqHeader.sequence = sequence;	
	reqHeader.payloadLen = payloadLen;

	// Send header
	ssize_t sent = send(clientSock, &reqHeader, sizeof(reqHeader), 0);
	if (sent != sizeof(reqHeader))
	{
		PROTEUS_LOG("Failed to send req header: %s", strerror(errno));
		return -1;
	}

	// Send payload
	if (payloadLen > 0 && payload != NULL)
	{
		sent = send(clientSock, payload, payloadLen, 0);
		if (sent != (ssize_t)payloadLen)
		{
			PROTEUS_LOG("Failed to send req payload: %s", strerror(errno));
			return -1;
		}
	}

	PROTEUS_LOG(">> cmd=%u, seq=%u, len=%u", command, sequence, payloadLen);
	return 0;
}

int proteus_recv_response(int clientSock, uint16_t expectedSequence,
	uint8_t* payload, uint16_t payloadBufSize, uint16_t* actualPayloadLen)
{
	// Receive response header
	ProteusRespHeader respHeader;
	ssize_t recvd = recv(clientSock, &respHeader, sizeof(respHeader), 0);

	if (recvd != sizeof(respHeader))
	{
		PROTEUS_LOG("Failed to receive resp header: %s", strerror(errno));
		return -1;
	}

	if (respHeader.magic != PROTEUS_MAGIC)
	{
		PROTEUS_LOG("Unexpected magic in resp header (0x%08X)", respHeader.magic);
		return -1;
	}

	if (respHeader.sequence != expectedSequence)
	{
		PROTEUS_LOG("Unexpected sequence in resp header (%u)", respHeader.sequence);
		return -1;
	}

	const char* statusStr = NULL;
	switch (respHeader.status)
	{
	case PROTEUS_STATUS_SUCCESS:	
		statusStr = "SUCCESS";
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
		statusStr = "UNKNOWN";
		break;
	}

	(void)statusStr;
	PROTEUS_LOG("<< %s (%d), seq=%u, len=%u", statusStr, respHeader.status,
		respHeader.sequence, respHeader.payloadLen);

	if (respHeader.status != PROTEUS_STATUS_SUCCESS)
	{
		return -1;
	}

	// Receive response payload
	if (respHeader.payloadLen > 0)
	{
		if (payload == NULL || actualPayloadLen == NULL)
		{
			PROTEUS_LOG("Failed to receive resp payload: invalid client arguments");
			return -1;
		}
		
		if (payloadBufSize < respHeader.payloadLen)
		{
			PROTEUS_LOG("Failed to receive resp payload: client buffer len=%u but got payload len=%u",
				payloadBufSize, respHeader.payloadLen);
			return -1;
		}

		uint16_t receivedBytes = 0;
		while (receivedBytes < respHeader.payloadLen)
		{
			recvd = recv(clientSock, payload + receivedBytes, respHeader.payloadLen - receivedBytes, 0);
			if (recvd <= 0)
			{
				PROTEUS_LOG("Failed to receive resp payload: %s", strerror(errno));
				return -1;
			}

			receivedBytes += recvd;
		}

		*actualPayloadLen = receivedBytes;
	}

	return 0;
}
