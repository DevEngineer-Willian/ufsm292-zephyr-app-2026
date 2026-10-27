/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <datalogger/datalogger.h>
#include <string.h>

ZTEST(datalogger_suite, test_datalogger_null_checks)
{
    datalogger_entry_t entry = { .timestamp_ms = 100, .raw_io_val = 50, .digital_state = true };
    char buf[64];

    zassert_equal(datalogger_format_json(NULL, buf, sizeof(buf)), -1, "Deve falhar se entry for NULL");
    zassert_equal(datalogger_format_json(&entry, NULL, sizeof(buf)), -1, "Deve falhar se buffer for NULL");
    zassert_equal(datalogger_format_json(&entry, buf, 0), -1, "Deve falhar se buf_len for 0");
}

ZTEST(datalogger_suite, test_datalogger_valid_json)
{
    datalogger_entry_t entry = {
        .timestamp_ms = 1234,
        .raw_io_val = 512,
        .digital_state = true
    };
    char buf[128];

    int ret = datalogger_format_json(&entry, buf, sizeof(buf));
    zassert_equal(ret, 0, "Deve retornar 0 para entrada valida");

    /* Verificacao simples de substrings no JSON gerado */
    zassert_not_null(strstr(buf, "\"ts\":1234"), "Deveria conter ts:1234");
    zassert_not_null(strstr(buf, "\"io_val\":512"), "Deveria conter io_val:512");
    zassert_not_null(strstr(buf, "\"state\":true"), "Deveria conter state:true");
}

ZTEST(datalogger_suite, test_datalogger_buffer_overflow)
{
    datalogger_entry_t entry = { .timestamp_ms = 99999, .raw_io_val = 1023, .digital_state = false };
    char tiny_buf[10]; /* Buffer insuficiente para o JSON */

    int ret = datalogger_format_json(&entry, tiny_buf, sizeof(tiny_buf));
    zassert_equal(ret, -1, "Deve falhar com erro de overflow quando o buffer for pequeno");
}

ZTEST_SUITE(datalogger_suite, NULL, NULL, NULL, NULL, NULL);
