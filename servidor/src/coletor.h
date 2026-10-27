#ifndef COLETOR_H
#define COLETOR_H

/*
 * Faz GET /sensors no gateway, atualiza a tabela e grava no SD só as
 * leituras novas. Retorna quantas eram novas ou erro negativo.
 */
int coletor_consultar(void);

#endif
