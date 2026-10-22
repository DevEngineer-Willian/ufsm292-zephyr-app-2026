#include "coletor.h"
#include "armazenamento.h"
#include "http_cliente.h"
#include "relogio.h"
#include "tabela.h"

#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

/* Mesmo limite do gateway (GATEWAY_MEMORY_MAX_NODES) */
#define MAX_NOS 4

/*
 * Formato de um item do GET /sensors (lib/gateway_json/sensors_json.c):
 * {"node_id":1,"light":320,"temp_c":24.53,"accel":[10,-20,980],"last_seen_ms":1234}
 * Campos que não estão aqui são ignorados pelo parser.
 */
struct sensor_json {
	int32_t node_id;
	int32_t light;
	struct json_obj_token temp_c; /* texto "24.53": convertido sem float */
	int32_t accel[3];
	size_t accel_len;
	int32_t last_seen_ms;
};

static const struct json_obj_descr sensor_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct sensor_json, node_id, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct sensor_json, light, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct sensor_json, temp_c, JSON_TOK_FLOAT),
	JSON_OBJ_DESCR_ARRAY(struct sensor_json, accel, 3, accel_len, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct sensor_json, last_seen_ms, JSON_TOK_NUMBER),
};

struct sensores_json {
	struct sensor_json nos[MAX_NOS];
	size_t n;
};

static const struct json_obj_descr sensores_descr[] = {
	JSON_OBJ_DESCR_OBJ_ARRAY(struct sensores_json, nos, MAX_NOS, n, sensor_descr,
				 ARRAY_SIZE(sensor_descr)),
};

/* Resposta do gateway: até ~515 B de corpo (4 nós) mais os cabeçalhos */
static char buf[640];
/* Só o main usa: fica fora da pilha */
static struct sensores_json sensores;

/* "24.53" -> 2453, "-3.5" -> -350, "24" -> 2400 */
static int16_t temp_x100(const struct json_obj_token *t)
{
	const char *p = t->start;
	const char *fim = p + t->length;
	bool negativo = false;
	int32_t inteiro = 0;
	int32_t frac = 0;
	int casas = 0;
	int32_t valor;

	if (p == NULL) {
		return 0;
	}
	if (p < fim && *p == '-') {
		negativo = true;
		p++;
	}
	while (p < fim && *p >= '0' && *p <= '9') {
		inteiro = inteiro * 10 + (*p++ - '0');
	}
	if (p < fim && *p == '.') {
		p++;
		while (p < fim && *p >= '0' && *p <= '9' && casas < 2) {
			frac = frac * 10 + (*p++ - '0');
			casas++;
		}
	}
	for (; casas < 2; casas++) {
		frac *= 10;
	}

	valor = inteiro * 100 + frac;
	return negativo ? -valor : valor;
}

int coletor_consultar(void)
{
	char *corpo = NULL;
	int novas = 0;
	int status;
	int ret;

	status = http_get(CONFIG_SERVIDOR_GATEWAY_ADDR, CONFIG_SERVIDOR_GATEWAY_PORTA,
			  "/sensors", buf, sizeof(buf), &corpo);
	if (status != 200) {
		printk("Coletor: GET /sensors falhou (%d)\r\n", status);
		return status < 0 ? status : -EIO;
	}

	memset(&sensores, 0, sizeof(sensores));
	ret = json_arr_parse(corpo, strlen(corpo), sensores_descr, &sensores);
	if (ret < 0) {
		printk("Coletor: JSON invalido (%d)\r\n", ret);
		return ret;
	}

	for (size_t i = 0; i < sensores.n; i++) {
		const struct sensor_json *s = &sensores.nos[i];
		struct leitura l = {
			.node_id = s->node_id,
			.light = s->light,
			.temp_c_x100 = temp_x100(&s->temp_c),
			.accel = { s->accel[0], s->accel[1], s->accel[2] },
			.last_seen_ms = s->last_seen_ms,
		};

		/* O gateway repete a última leitura até o nó mandar outra */
		if (!tabela_atualizar(&l)) {
			continue;
		}

		novas++;
		ret = armazenamento_gravar(&l);
		if (ret < 0) {
			printk("SD: falha ao gravar o no %u (%d)\r\n", l.node_id, ret);
		} else {
			char data_hora[20];

			relogio_agora(data_hora, sizeof(data_hora));
			printk("SD: leitura nova do no %u gravada %s\r\n", l.node_id,
			       data_hora);
		}
	}

	return novas;
}
