#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "api_http.h"
#include "armazenamento.h"
#include "rede.h"
#include "relogio.h"

/* O relógio da placa deriva com o tempo: refaz o NTP de hora em hora */
#define RESYNC_NTP_MS (60 * 60 * 1000)
/* Sem internet no boot, tenta o NTP de novo a cada minuto */
#define RETRY_NTP_MS (60 * 1000)

static void manter_relogio(void)
{
	static int64_t proxima_ms;
	int64_t agora = k_uptime_get();

	if (agora < proxima_ms) {
		return;
	}

	if (relogio_sincronizar() == 0) {
		proxima_ms = agora + RESYNC_NTP_MS;
	} else {
		printk("Relogio: sem NTP, data_hora fica vazia por enquanto\r\n");
		proxima_ms = agora + RETRY_NTP_MS;
	}
}

int main(void)
{
	printk("\r\nServidor (grupo C)\r\n");

	rede_iniciar();

	/* Sem SD o servidor continua no ar: o supervisor ainda precisa do /health */
	if (armazenamento_iniciar() < 0) {
		printk("SD: indisponivel, leituras nao serao gravadas\r\n");
	}

	/* Escuta em qualquer IP: começa antes do DHCP e atende quando ele chegar */
	api_http_iniciar();

	printk("Rede: aguardando DHCP...\r\n");
	while (rede_aguardar_ip(K_SECONDS(10)) < 0) {
		printk("Rede: ainda sem IP, confira o cabo\r\n");
	}

	/*
	 * TEMPORARIO: leituras fictícias no lugar do coletor, que vai buscar
	 * as leituras reais no GET /sensors do gateway.
	 */
	for (uint32_t i = 0;; i++) {
		struct leitura l = {
			.node_id = 1 + (i % 3),
			.light = 300 + (i % 50),
			.temp_c = 24.5f + 0.1f * (i % 10),
			.accel = { 0.01f, 0.02f, 9.8f },
			.last_seen_ms = (uint32_t)k_uptime_get() - 100,
		};
		char data_hora[20];
		int ret;

		manter_relogio();

		ret = armazenamento_gravar(&l);
		if (ret < 0) {
			printk("SD: falha ao gravar (%d)\r\n", ret);
		} else {
			relogio_agora(data_hora, sizeof(data_hora));
			printk("SD: leitura do no %d gravada %s\r\n", l.node_id,
			       data_hora);
		}
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
