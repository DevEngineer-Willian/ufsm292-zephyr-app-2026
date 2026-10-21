#ifndef REDE_H
#define REDE_H

#include <zephyr/kernel.h>

/* Registra os eventos de rede. O DHCP começa sozinho quando o link sobe. */
void rede_iniciar(void);

/* Espera a placa ter um IPv4. Retorna 0 com IP ou -EAGAIN no timeout. */
int rede_aguardar_ip(k_timeout_t timeout);

#endif
