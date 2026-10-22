#include "relogio.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <time.h>

/*
 * Servidores do NTP.br por IP: usar nome exigiria o resolvedor de DNS, que
 * custa ~3 KB de RAM. Tenta um de cada vez até algum responder.
 */
static const char *const servidores_ntp[] = {
	"200.160.7.186",  /* a.st1.ntp.br */
	"201.49.148.135", /* b.st1.ntp.br */
	"200.160.0.8",    /* a.ntp.br */
};

#define TIMEOUT_NTP_MS 3000
/* Brasília é UTC-3 o ano todo (sem horário de verão desde 2019) */
#define FUSO_BRASILIA_S (-3 * 3600)

/* Instante Unix, em ms, em que a placa ligou (uptime 0) */
static int64_t epoca_no_boot_ms;
static bool sincronizado;

int relogio_sincronizar(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(servidores_ntp); i++) {
		struct net_sockaddr_in addr = {
			.sin_family = NET_AF_INET,
			.sin_port = net_htons(123),
		};
		struct sntp_time ts;
		char texto[20];
		int ret;

		net_addr_pton(NET_AF_INET, servidores_ntp[i], &addr.sin_addr);

		ret = sntp_simple_addr((struct net_sockaddr *)&addr, sizeof(addr),
				       TIMEOUT_NTP_MS, &ts);
		if (ret < 0) {
			printk("Relogio: %s nao respondeu (%d)\r\n",
			       servidores_ntp[i], ret);
			continue;
		}

		/* fraction é a parte fracionária do segundo em 1/2^32 */
		epoca_no_boot_ms = (int64_t)ts.seconds * 1000 +
				   (((uint64_t)ts.fraction * 1000) >> 32) -
				   k_uptime_get();
		sincronizado = true;

		relogio_agora(texto, sizeof(texto));
		printk("Relogio: %s (horario de Brasilia)\r\n", texto);
		return 0;
	}

	return -ETIMEDOUT;
}

bool relogio_sincronizado(void)
{
	return sincronizado;
}

bool relogio_agora(char *buf, size_t tam)
{
	time_t agora;
	struct tm tm;

	if (!sincronizado) {
		buf[0] = '\0';
		return false;
	}

	agora = (time_t)((epoca_no_boot_ms + k_uptime_get()) / 1000) +
		FUSO_BRASILIA_S;
	gmtime_r(&agora, &tm);
	strftime(buf, tam, "%Y-%m-%d %H:%M:%S", &tm);
	return true;
}
