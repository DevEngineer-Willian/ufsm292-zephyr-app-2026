#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "api_http.h"
#include "armazenamento.h"
#include "coletor.h"
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

	printk("Coletor: gateway em %s:%d, a cada %d ms\r\n",
	       CONFIG_SERVIDOR_GATEWAY_ADDR, CONFIG_SERVIDOR_GATEWAY_PORTA,
	       CONFIG_SERVIDOR_COLETA_MS);

	/* O coletor roda aqui no main, sem thread própria: economiza RAM */
	for (;;) {
		manter_relogio();
		coletor_consultar();
		k_sleep(K_MSEC(CONFIG_SERVIDOR_COLETA_MS));
	}

	return 0;
}
