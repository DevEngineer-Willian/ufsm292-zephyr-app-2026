#ifndef LEITURA_H
#define LEITURA_H

#include <stdint.h>

/* Uma leitura de um nó sensor, no mesmo formato de um item do GET /sensors */
struct leitura {
	uint8_t node_id;
	uint16_t light;
	float temp_c;
	float accel[3];
	uint32_t last_seen_ms;
};

#endif
