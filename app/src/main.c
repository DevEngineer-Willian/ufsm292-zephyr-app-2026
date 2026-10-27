/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <datalogger/datalogger.h>

int main(void)
{
    printk("=== SAMD21 DataLogger Subsystem Initialized ===\n");

    datalogger_init();

    datalogger_entry_t sample = {
        .timestamp_ms = 0,
        .raw_io_val = 100,
        .digital_state = true
    };

    char json_buf[128];

    while (1) {
        sample.timestamp_ms = k_uptime_get_32();
        sample.raw_io_val = (sample.raw_io_val + 15) % 1024;
        sample.digital_state = !sample.digital_state;

        if (datalogger_format_json(&sample, json_buf, sizeof(json_buf)) == 0) {
            printk("[DataLogger] JSON Output: %s\n", json_buf);
        } else {
            printk("[DataLogger] Error formatting JSON!\n");
        }

        k_sleep(K_MSEC(2000));
    }

    return 0;
}
