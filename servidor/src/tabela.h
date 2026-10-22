#ifndef TABELA_H
#define TABELA_H

#include <stddef.h>

#include "leitura.h"

#include <stdbool.h>

/*
 * Guarda a leitura como a mais recente do nó. Retorna false se ela é a mesma
 * que já estava (mesmo last_seen_ms): o gateway repete a última leitura até
 * o nó mandar outra, e ela não deve ser gravada de novo.
 */
bool tabela_atualizar(const struct leitura *l);

/* Nós com leitura recebida há no máximo CONFIG_SERVIDOR_NO_ONLINE_MS */
int tabela_nos_online(void);

/*
 * Escreve as últimas leituras como array JSON, no formato do GET /sensors.
 * Retorna o tamanho escrito ou -ENOMEM se não couber em buf.
 */
int tabela_json(char *buf, size_t tam);

#endif
