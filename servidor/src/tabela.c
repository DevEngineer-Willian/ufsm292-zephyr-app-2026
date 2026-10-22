#include "tabela.h"

#include <zephyr/kernel.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

/* Mesmo limite do gateway (GATEWAY_MEMORY_MAX_NODES) */
#define MAX_NOS 4

struct entrada {
	struct leitura l;
	int64_t recebido_ms; /* uptime do servidor quando a leitura chegou */
	bool usada;
};

/* O coletor escreve e a thread HTTP lê */
static K_MUTEX_DEFINE(trava);
static struct entrada tabela[MAX_NOS];

bool tabela_atualizar(const struct leitura *l)
{
	struct entrada *alvo = NULL;

	k_mutex_lock(&trava, K_FOREVER);

	for (int i = 0; i < MAX_NOS; i++) {
		if (tabela[i].usada && tabela[i].l.node_id == l->node_id) {
			alvo = &tabela[i];
			break;
		}
	}

	if (alvo != NULL && alvo->l.last_seen_ms == l->last_seen_ms) {
		k_mutex_unlock(&trava);
		return false;
	}

	/* Nó novo: vaga livre ou, com a tabela cheia, a leitura mais antiga */
	if (alvo == NULL) {
		alvo = &tabela[0];
		for (int i = 0; i < MAX_NOS; i++) {
			if (!tabela[i].usada) {
				alvo = &tabela[i];
				break;
			}
			if (tabela[i].recebido_ms < alvo->recebido_ms) {
				alvo = &tabela[i];
			}
		}
	}

	alvo->l = *l;
	alvo->recebido_ms = k_uptime_get();
	alvo->usada = true;

	k_mutex_unlock(&trava);
	return true;
}

int tabela_nos_online(void)
{
	int64_t agora = k_uptime_get();
	int online = 0;

	k_mutex_lock(&trava, K_FOREVER);
	for (int i = 0; i < MAX_NOS; i++) {
		if (tabela[i].usada &&
		    agora - tabela[i].recebido_ms <= CONFIG_SERVIDOR_NO_ONLINE_MS) {
			online++;
		}
	}
	k_mutex_unlock(&trava);

	return online;
}

int tabela_json(char *buf, size_t tam)
{
	size_t usado = 0;
	bool primeiro = true;
	int ret = 0;
	int n;

	if (tam < 3) {
		return -ENOMEM;
	}
	buf[usado++] = '[';

	k_mutex_lock(&trava, K_FOREVER);
	for (int i = 0; i < MAX_NOS; i++) {
		const struct leitura *l = &tabela[i].l;

		if (!tabela[i].usada) {
			continue;
		}

		n = snprintf(buf + usado, tam - usado,
			     "%s{\"node_id\":%u,\"light\":%u,\"temp_c\":" TEMP_FMT
			     ",\"accel\":[%d,%d,%d],\"last_seen_ms\":%u}",
			     primeiro ? "" : ",", l->node_id, l->light,
			     TEMP_ARGS(l->temp_c_x100), l->accel[0], l->accel[1],
			     l->accel[2], l->last_seen_ms);
		if (n < 0 || (size_t)n >= tam - usado) {
			ret = -ENOMEM;
			break;
		}
		usado += n;
		primeiro = false;
	}
	k_mutex_unlock(&trava);

	if (ret < 0 || usado + 2 > tam) {
		return -ENOMEM;
	}
	buf[usado++] = ']';
	buf[usado] = '\0';
	return usado;
}
