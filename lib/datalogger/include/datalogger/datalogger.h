/*
 * SPDX-License-Identifier: Apache-2.0
 * Module: DataLogger for SAMD21 + Ethernet/IO
 */

#ifndef DATALOGGER_H_
#define DATALOGGER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Estrutura de leitura dos dados do registrador/sensores.
 */
typedef struct {
    uint32_t timestamp_ms; /**< Timestamp da leitura em milissegundos */
    int32_t raw_io_val;     /**< Leitura bruta de entrada/saída do IO1 */
    bool digital_state;     /**< Estado lógico de entrada do IO1 */
} datalogger_entry_t;

/**
 * @brief Inicializa o buffer/instância do DataLogger.
 * @return 0 em caso de sucesso, número negativo em erro.
 */
int datalogger_init(void);

/**
 * @brief Formata uma entrada de dados no formato JSON para transmissão Ethernet/Serial.
 * 
 * @param entry Ponteiro para a amostra lida.
 * @param out_buf Buffer de destino para a string JSON.
 * @param buf_len Tamanho máximo do buffer de destino.
 * @return 0 em caso de sucesso, -1 se houver estouro de buffer ou dados inválidos.
 */
int datalogger_format_json(const datalogger_entry_t *entry, char *out_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* DATALOGGER_H_ */
