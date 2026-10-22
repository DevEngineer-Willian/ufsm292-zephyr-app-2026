#ifndef HTTP_CLIENTE_H
#define HTTP_CLIENTE_H

#include <stddef.h>
#include <stdint.h>

/*
 * GET http://ip:porta/caminho. A resposta inteira (cabeçalhos + corpo) fica
 * em buf e *corpo aponta para o corpo dentro dele.
 * Retorna o código HTTP (200...) ou erro negativo (-EAGAIN = timeout).
 */
int http_get(const char *ip, uint16_t porta, const char *caminho, char *buf,
	     size_t tam, char **corpo);

#endif
