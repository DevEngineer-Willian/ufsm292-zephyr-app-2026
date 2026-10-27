/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <datalogger/datalogger.h>
#include <stdio.h>
#include <string.h>

#if __has_include(<cJSON.h>)
  #include <cJSON.h>
  #define HAS_CJSON 1
#elif __has_include(<cjson/cJSON.h>)
  #include <cjson/cJSON.h>
  #define HAS_CJSON 1
#else
  #define HAS_CJSON 0
#endif

int datalogger_init(void)
{
    return 0;
}

int datalogger_format_json(const datalogger_entry_t *entry, char *out_buf, size_t buf_len)
{
    if (!entry || !out_buf || buf_len == 0) {
        return -1;
    }

#if HAS_CJSON
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return -1;
    }

    cJSON_AddNumberToObject(root, "ts", entry->timestamp_ms);
    cJSON_AddNumberToObject(root, "io_val", entry->raw_io_val);
    cJSON_AddBoolToObject(root, "state", entry->digital_state);

    char *rendered = cJSON_PrintUnformatted(root);
    if (!rendered) {
        cJSON_Delete(root);
        return -1;
    }

    if (strlen(rendered) >= buf_len) {
        cJSON_free(rendered);
        cJSON_Delete(root);
        return -1;
    }

    strncpy(out_buf, rendered, buf_len);
    cJSON_free(rendered);
    cJSON_Delete(root);
    return 0;

#else
    /* Fallback nativo e determinístico para JSON em sistemas embarcados */
    int ret = snprintf(out_buf, buf_len,
                       "{\"ts\":%u,\"io_val\":%d,\"state\":%s}",
                       (unsigned int)entry->timestamp_ms,
                       (int)entry->raw_io_val,
                       entry->digital_state ? "true" : "false");

    if (ret < 0 || (size_t)ret >= buf_len) {
        return -1;
    }

    return 0;
#endif
}
