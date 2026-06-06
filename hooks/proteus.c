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

#include "common.h"

//=============================================================================
// Internal data
//=============================================================================

static int s_emulator_socket = -1;
static pthread_mutex_t s_socket_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t s_sequence = 0;
static pthread_mutex_t s_sequence_mutex = PTHREAD_MUTEX_INITIALIZER;

//=============================================================================
// Init / Destroy
//=============================================================================

void proteus_init()
{
	s_emulator_socket = -1;
	s_sequence = 0;
}

void proteus_destroy()
{
	pthread_mutex_destroy(&s_socket_mutex);
	pthread_mutex_destroy(&s_sequence_mutex);
}

//=============================================================================
// Socket communication
//=============================================================================

static int proteus_resolve_host(const char* host, struct sockaddr_in* addr)
{
	struct hostent* he = gethostbyname(host);
	if (!he)
	{
		PROTEUS_LOG("Failed to resolve host: %s", host);
		return -1;
	}

	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(g_proteus_config.emulator_port);
	memcpy(&addr->sin_addr, he->h_addr_list[0], he->h_length);

	return 0;
}

uint32_t proteus_next_sequence()
{
	pthread_mutex_lock(&s_sequence_mutex);
	uint32_t seq = ++s_sequence;
	pthread_mutex_unlock(&s_sequence_mutex);
	return seq;
}

int proteus_connect()
{
	pthread_mutex_lock(&s_socket_mutex);

	if (s_emulator_socket >= 0)
	{
		pthread_mutex_unlock(&s_socket_mutex);
		return s_emulator_socket;
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		PROTEUS_LOG("Failed to create socket: %s", strerror(errno));
		pthread_mutex_unlock(&s_socket_mutex);
		return -1;
	}

	struct sockaddr_in addr;
	if (proteus_resolve_host(g_proteus_config.emulator_host, &addr) < 0)
	{
		close(sock);
		pthread_mutex_unlock(&s_socket_mutex);
		return -1;
	}

	// Set socket timeout
	struct timeval timeout;
	timeout.tv_sec = PROTEUS_SOCKET_TIMEOUT_MS / 1000;
	timeout.tv_usec = (PROTEUS_SOCKET_TIMEOUT_MS % 1000) * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	// Attempt connection with retries
#define RETRY_COUNT (3)

	int retry = RETRY_COUNT;
	while (retry-- > 0)
	{
		if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0)
		{
			PROTEUS_LOG("Connected to emulator at %s:%d",
				g_proteus_config.emulator_host,
				g_proteus_config.emulator_port);
			s_emulator_socket = sock;
			pthread_mutex_unlock(&s_socket_mutex);
			return sock;
		}

		usleep(100000); // 100ms
	}

	PROTEUS_LOG("Failed to connect to %s:%d, timeout: %d ms, retry: %d (%s)",
		g_proteus_config.emulator_host, g_proteus_config.emulator_port,
		PROTEUS_SOCKET_TIMEOUT_MS, RETRY_COUNT, strerror(errno));
	close(sock);
	pthread_mutex_unlock(&s_socket_mutex);

	return -1;
}

int proteus_send_transaction(uint32_t msg_type, uint32_t sequence,
	const uint8_t* payload, uint32_t payload_len)
{
	int sock = proteus_connect();
	if (sock < 0)
	{
		return -1;
	}

	pthread_mutex_lock(&s_socket_mutex);

	ProteusHeader header;
	header.magic = PROTEUS_MAGIC;
	header.msg_type = msg_type;
	header.sequence = sequence;
	header.payload_len = payload_len;

	// Send header
	ssize_t sent = send(sock, &header, sizeof(header), 0);
	if (sent != sizeof(header))
	{
		PROTEUS_LOG("Failed to send header: %s", strerror(errno));
		pthread_mutex_unlock(&s_socket_mutex);
		return -1;
	}

	// Send payload
	if (payload_len > 0 && payload)
	{
		sent = send(sock, payload, payload_len, 0);
		if (sent != (ssize_t)payload_len)
		{
			PROTEUS_LOG("Failed to send payload: %s", strerror(errno));
			pthread_mutex_unlock(&s_socket_mutex);
			return -1;
		}
	}

	pthread_mutex_unlock(&s_socket_mutex);
	PROTEUS_LOG(">> msg=%u, seq=%u, payload=%u bytes",
		msg_type, sequence, payload_len);

	return 0;
}

int proteus_recv_response(uint8_t* read_data, uint32_t* read_len)
{
	int sock = s_emulator_socket;
	if (sock < 0)
	{
		return -1;
	}

	pthread_mutex_lock(&s_socket_mutex);

	// Receive footer first
	ProteusFooter footer;
	ssize_t recvd = recv(sock, &footer, sizeof(footer), 0);

	if (recvd != sizeof(footer))
	{
		PROTEUS_LOG("Failed to receive footer: %s", strerror(errno));
		pthread_mutex_unlock(&s_socket_mutex);
		return -1;
	}

	if (footer.status != PROTEUS_STATUS_SUCCESS)
	{
		const char* statusStr = NULL;
		switch (footer.status)
		{
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
		PROTEUS_LOG("Transaction failed: %s (%d)", statusStr, footer.status);

		pthread_mutex_unlock(&s_socket_mutex);
		return -1;
	}

	// Receive read data if any
	if (read_data && read_len && *read_len > 0)
	{
		uint32_t bytes_received = 0;
		while (bytes_received < *read_len)
		{
			recvd = recv(sock, read_data + bytes_received,
				*read_len - bytes_received, 0);
			if (recvd <= 0)
			{
				PROTEUS_LOG("Failed to receive read data: %s", strerror(errno));
				pthread_mutex_unlock(&s_socket_mutex);
				return -1;
			}

			bytes_received += recvd;
		}

		PROTEUS_LOG("Received %u bytes of read data", bytes_received);
	}

	pthread_mutex_unlock(&s_socket_mutex);
	return 0;
}

void proteus_disconnect()
{
	pthread_mutex_lock(&s_socket_mutex);

	if (s_emulator_socket >= 0)
	{
		close(s_emulator_socket);
		s_emulator_socket = -1;

		PROTEUS_LOG("Disconnected from emulator");
	}

	pthread_mutex_unlock(&s_socket_mutex);
}

