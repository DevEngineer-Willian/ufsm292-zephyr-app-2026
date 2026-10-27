/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <datalogger/datalogger.h>
#include <stdio.h>
#include <string.h>

void datalogger_buf_init(datalogger_ring_buf_t *ring)
{
    if (!ring) return;
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

int datalogger_buf_push(datalogger_ring_buf_t *ring, const datalogger_entry_t *entry)
{
    if (!ring || !entry) {
        return -1;
    }

    /* Insere na posicao head */
    ring->entries[ring->head] = *entry;
    ring->head = (ring->head + 1) % DATALOGGER_BUFFER_CAPACITY;

    if (ring->count < DATALOGGER_BUFFER_CAPACITY) {
        ring->count++;
    } else {
        /* Buffer cheio: avanca o tail (sobrescreve o item mais antigo) */
        ring->tail = (ring->tail + 1) % DATALOGGER_BUFFER_CAPACITY;
    }

    return 0;
}

int datalogger_buf_pop(datalogger_ring_buf_t *ring, datalogger_entry_t *out_entry)
{
    if (!ring || !out_entry || ring->count == 0) {
        return -1;
    }

    *out_entry = ring->entries[ring->tail];
    ring->tail = (ring->tail + 1) % DATALOGGER_BUFFER_CAPACITY;
    ring->count--;

    return 0;
}

size_t datalogger_buf_count(const datalogger_ring_buf_t *ring)
{
    return ring ? ring->count : 0;
}

int datalogger_format_json(const datalogger_entry_t *entry, char *out_buf, size_t buf_len)
{
    if (!entry || !out_buf || buf_len == 0) {
        return -1;
    }

    int ret = snprintf(out_buf, buf_len,
                       "{\"ts\":%u,\"io_val\":%d,\"state\":%s}",
                       (unsigned int)entry->timestamp_ms,
                       (int)entry->raw_io_val,
                       entry->digital_state ? "true" : "false");

    if (ret < 0 || (size_t)ret >= buf_len) {
        return -1;
    }

    return 0;
}
