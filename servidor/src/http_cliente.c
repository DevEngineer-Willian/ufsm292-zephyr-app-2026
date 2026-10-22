/*
 * Cliente HTTP mínimo: uma requisição por conexão. O gateway responde com
 * Content-Length e Connection: close, então basta ler até ele fechar.
 */

#include "http_cliente.h"

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMEOUT_RECV_S 2

int http_get(const char *ip, uint16_t porta, const char *caminho, char *buf,
	     size_t tam, char **corpo)
{
	struct net_sockaddr_in addr = {
		.sin_family = NET_AF_INET,
		.sin_port = net_htons(porta),
	};
	struct zsock_timeval tv = { .tv_sec = TIMEOUT_RECV_S };
	size_t usado = 0;
	ssize_t n = 0;
	char *sep;
	int sock;
	int len;
	int ret;

	if (net_addr_pton(NET_AF_INET, ip, &addr.sin_addr) < 0) {
		return -EINVAL;
	}

	sock = zsock_socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);
	if (sock < 0) {
		return -errno;
	}
	zsock_setsockopt(sock, ZSOCK_SOL_SOCKET, ZSOCK_SO_RCVTIMEO, &tv, sizeof(tv));

	len = snprintf(buf, tam,
		       "GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n",
		       caminho, ip, porta);

	if (zsock_connect(sock, (struct net_sockaddr *)&addr, sizeof(addr)) < 0 ||
	    zsock_send(sock, buf, len, 0) != len) {
		ret = errno != 0 ? -errno : -EIO;
		goto fim;
	}

	while (usado < tam - 1) {
		n = zsock_recv(sock, buf + usado, tam - 1 - usado, 0);
		if (n <= 0) {
			break;
		}
		usado += n;
	}
	buf[usado] = '\0';

	if (n < 0) {
		ret = -errno;
	} else if (usado == tam - 1) {
		ret = -ENOBUFS;
	} else if (strncmp(buf, "HTTP/1.", 7) != 0 ||
		   (sep = strstr(buf, "\r\n\r\n")) == NULL) {
		ret = -EBADMSG;
	} else {
		*corpo = sep + 4;
		ret = atoi(buf + 9); /* "HTTP/1.1 200 OK" */
	}

fim:
	zsock_close(sock);
	return ret;
}
