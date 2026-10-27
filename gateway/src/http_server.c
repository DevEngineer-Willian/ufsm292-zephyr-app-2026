/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/parser.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <app/gateway/sensors_json.h>
#include <app/gateway/sensors_response.h>
#include "http_server.h"

LOG_MODULE_DECLARE(gateway, CONFIG_GATEWAY_LOG_LEVEL);

/* One connection at a time; storage remains owned until send completes. */
static char response[GATEWAY_SENSORS_JSON_MAX_SIZE];

struct request {
	char path[32];
	size_t path_len;
	bool complete;
};

static void reboot_gateway(struct k_work *work)
{
	ARG_UNUSED(work);
	sys_reboot(SYS_REBOOT_COLD);
}

static K_WORK_DELAYABLE_DEFINE(reset_work, reboot_gateway);

static int request_url(struct http_parser *parser, const char *data, size_t len)
{
	struct request *request = parser->data;

	if (len >= sizeof(request->path) - request->path_len) {
		return -1;
	}
	memcpy(request->path + request->path_len, data, len);
	request->path_len += len;
	request->path[request->path_len] = '\0';
	return 0;
}

static int request_complete(struct http_parser *parser)
{
	struct request *request = parser->data;

	request->complete = true;
	/* Ignore pipelined requests: every response closes the connection. */
	http_parser_pause(parser, 1);
	return 0;
}

static const struct http_parser_settings parser_settings = {
	.on_url = request_url,
	.on_message_complete = request_complete,
};

static int send_all(int fd, const char *data, size_t len)
{
	while (len > 0U) {
		ssize_t sent = zsock_send(fd, data, len, 0);

		if (sent < 0) {
			return -errno;
		}
		if (sent == 0) {
			return -EIO;
		}
		data += sent;
		len -= sent;
	}
	return 0;
}

static int send_response(int fd, unsigned int status, const char *reason,
			 const char *body, size_t len, const char *allow)
{
	char header[192];
	int count = snprintk(header, sizeof(header),
		"HTTP/1.1 %u %s\r\nContent-Type: application/json\r\n"
		"Content-Length: %u\r\nConnection: close\r\n%s\r\n",
		status, reason, (unsigned int)len, allow);
	int ret;

	if (count < 0 || (size_t)count >= sizeof(header)) {
		return -ENOSPC;
	}
	ret = send_all(fd, header, count);
	return ret < 0 ? ret : send_all(fd, body, len);
}

static void wait_for_peer_close(int fd)
{
	struct zsock_pollfd pollfd = { .fd = fd, .events = ZSOCK_POLLIN };
	int64_t deadline = k_uptime_get() + 1000;
	char discard[128];

	/* Content-Length lets clients finish without waiting for our FIN.
	 * Prefer a passive close so normal requests do not occupy TIME_WAIT
	 * contexts here. Peers waiting for EOF still get a bounded active close.
	 * Discard pipelined data without interpreting another request.
	 */
	while (true) {
		int64_t remaining = deadline - k_uptime_get();

		if (remaining <= 0 || zsock_poll(&pollfd, 1, (int)remaining) <= 0 ||
		    (pollfd.revents & (ZSOCK_POLLIN | ZSOCK_POLLHUP)) == 0) {
			return;
		}
		if (zsock_recv(fd, discard, sizeof(discard), ZSOCK_MSG_DONTWAIT) <= 0) {
			return;
		}
	}
}
static void reject_overload(int listener)
{
	struct timeval timeout = { .tv_usec = 250000 };
	int client = zsock_accept(listener, NULL, NULL);
	static const char body[] = "{\"error\":\"busy\"}";

	if (client < 0) {
		return;
	}
	if (zsock_setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout,
			     sizeof(timeout)) == 0) {
		LOG_WRN("http: overload");
		if (send_response(client, 500, "Internal Server Error",
				  body, sizeof(body) - 1U, "") == 0) {
			wait_for_peer_close(client);
		}
	}
	zsock_close(client);
}

static void serve_client(int fd, int listener)
{
	struct http_parser parser;
	struct request request = { 0 };
	struct zsock_pollfd pollfds[] = {
		{ .fd = fd, .events = ZSOCK_POLLIN },
		{ .fd = listener, .events = ZSOCK_POLLIN },
	};
	struct timeval timeout = { .tv_sec = 5 };
	int64_t deadline = k_uptime_get() + 10000;
	char input[128];
	size_t received = 0;
	bool reset = false;
	int len;

	if (zsock_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
		return;
	}
	http_parser_init(&parser, HTTP_REQUEST);
	parser.data = &request;
	while (!request.complete) {
		int64_t remaining = deadline - k_uptime_get();
		int ret;

		if (remaining <= 0) {
			return;
		}
		ret = zsock_poll(pollfds, ARRAY_SIZE(pollfds), (int)remaining);
		if (ret <= 0) {
			return;
		}
		if ((pollfds[0].revents & ZSOCK_POLLIN) == 0) {
			if (pollfds[0].revents != 0) {
				return;
			}
			if ((pollfds[1].revents & ZSOCK_POLLIN) != 0) {
				reject_overload(listener);
			}
			continue;
		}
		len = zsock_recv(fd, input, sizeof(input), 0);
		if (len <= 0) {
			return;
		}
		received += len;
		if (received > 2048U) {
			send_response(fd, 413, "Content Too Large", "{}", 2, "");
			return;
		}
		http_parser_execute(&parser, &parser_settings, input, len);
		if (HTTP_PARSER_ERRNO(&parser) != HPE_OK &&
		    HTTP_PARSER_ERRNO(&parser) != HPE_PAUSED) {
			send_response(fd, 400, "Bad Request", "{}", 2, "");
			return;
		}
	}

	LOG_INF("http: %s %s", http_method_str(parser.method), request.path);

	if (strcmp(request.path, "/sensors") == 0 || strcmp(request.path, "/health") == 0) {
		if (parser.method != HTTP_GET) {
			send_response(fd, 405, "Method Not Allowed", "{}", 2, "Allow: GET\r\n");
			return;
		}
		len = strcmp(request.path, "/sensors") == 0 ?
			gateway_sensors_json_response(response, sizeof(response)) :
			gateway_health_json_response(response, sizeof(response));
	} else if (strcmp(request.path, "/reset") == 0) {
		if (parser.method != HTTP_POST) {
			send_response(fd, 405, "Method Not Allowed", "{}", 2, "Allow: POST\r\n");
			return;
		}
		reset = true;
		memcpy(response, "{\"status\":\"restarting\"}", sizeof("{\"status\":\"restarting\"}"));
		len = sizeof("{\"status\":\"restarting\"}") - 1;
	} else {
		send_response(fd, 404, "Not Found", "{}", 2, "");
		return;
	}
	if (len < 0) {
		send_response(fd, 500, "Internal Server Error", "{}", 2, "");
		return;
	}
	if (send_response(fd, reset ? 202 : 200, reset ? "Accepted" : "OK",
			  response, len, "") == 0 && reset) {
		/* Queued successfully, not proof of delivery or a TCP acknowledgment. */
		k_work_schedule(&reset_work, K_MSEC(100));
	}
}

int gateway_http_run(void)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(CONFIG_GATEWAY_HTTP_PORT),
		.sin_addr.s_addr = INADDR_ANY,
	};
	int listener = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int ret;

	if (listener < 0) {
		return -errno;
	}
	if (zsock_bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0 ||
	    zsock_listen(listener, 1) < 0) {
		ret = -errno;
		zsock_close(listener);
		return ret;
	}
	while (true) {
		int client = zsock_accept(listener, NULL, NULL);

		if (client < 0) {
			if (errno == EINTR) {
				continue;
			}
			ret = -errno;
			break;
		}
		serve_client(client, listener);
		wait_for_peer_close(client);
		zsock_close(client);
	}
	zsock_close(listener);
	return ret;
}
