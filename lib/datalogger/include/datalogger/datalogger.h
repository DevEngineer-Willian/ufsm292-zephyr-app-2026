/*
 * SPDX-License-Identifier: Apache-2.0
 * Module: DataLogger Ring Buffer (RAM/NVS)
 */

#ifndef DATALOGGER_H_
#define DATALOGGER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATALOGGER_BUFFER_CAPACITY 16

typedef struct {
    uint32_t timestamp_ms;
    int32_t raw_io_val;
    bool digital_state;
} datalogger_entry_t;

typedef struct {
    datalogger_entry_t entries[DATALOGGER_BUFFER_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
} datalogger_ring_buf_t;

/**
 * @brief Inicializa a estrutura do buffer circular.
 */
void datalogger_buf_init(datalogger_ring_buf_t *ring);

/**
 * @brief Adiciona uma nova amostra ao buffer (sobrescreve a mais antiga se cheio).
 */
int datalogger_buf_push(datalogger_ring_buf_t *ring, const datalogger_entry_t *entry);

/**
 * @brief Retira a amostra mais antiga do buffer.
 */
int datalogger_buf_pop(datalogger_ring_buf_t *ring, datalogger_entry_t *out_entry);

/**
 * @brief Retorna a quantidade de amostras armazenadas no buffer.
 */
size_t datalogger_buf_count(const datalogger_ring_buf_t *ring);

/**
 * @brief Formata uma entrada de dados em JSON.
 */
int datalogger_format_json(const datalogger_entry_t *entry, char *out_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* DATALOGGER_H_ */
