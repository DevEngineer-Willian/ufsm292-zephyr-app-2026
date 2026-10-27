#ifndef RELOGIO_H
#define RELOGIO_H

#include <stdbool.h>
#include <stddef.h>

/* Busca a hora real num servidor NTP. Precisa de IP e acesso à internet. */
int relogio_sincronizar(void);

bool relogio_sincronizado(void);

/*
 * Escreve a hora atual como "AAAA-MM-DD HH:MM:SS" (horário de Brasília).
 * Sem sincronização, deixa buf vazio e retorna false.
 */
bool relogio_agora(char *buf, size_t tam);

#endif
