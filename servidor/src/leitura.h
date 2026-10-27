#ifndef LEITURA_H
#define LEITURA_H

#include <stdint.h>
#include <stdlib.h>

/*
 * Uma leitura de um nó sensor, nos mesmos tipos que o gateway usa no
 * GET /sensors (lib/gateway_json/sensors_json.c): tudo inteiro, sem float.
 */
struct leitura {
	uint8_t node_id;
	uint16_t light;
	int16_t temp_c_x100;  /* 2453 = 24.53 °C */
	int16_t accel[3];     /* valores brutos do sensor */
	uint32_t last_seen_ms; /* uptime do gateway na última recepção */
};

/*
 * Formata temp_c_x100 como "24.53" sem printf de float, que custaria flash e
 * pilha. Uso: printf("temp=" TEMP_FMT, TEMP_ARGS(l->temp_c_x100)).
 */
#define TEMP_FMT "%s%u.%02u"
#define TEMP_ARGS(x)                                                           \
	((x) < 0 ? "-" : ""), (unsigned int)(abs(x) / 100),                     \
		(unsigned int)(abs(x) % 100)

#endif
