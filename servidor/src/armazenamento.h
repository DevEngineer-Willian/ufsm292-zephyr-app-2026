#ifndef ARMAZENAMENTO_H
#define ARMAZENAMENTO_H

#include <stdbool.h>

#include "leitura.h"

/* Monta o SD e cria o LEITURAS.CSV com cabeçalho se ele ainda não existir */
int armazenamento_iniciar(void);

/* O SD está montado e pronto para gravar? */
bool armazenamento_ok(void);

/* Acrescenta uma linha ao CSV, com o instante em que o servidor a recebeu */
int armazenamento_gravar(const struct leitura *l);

#endif
