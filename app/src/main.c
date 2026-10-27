/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <datalogger/datalogger.h>

#define SW0_NODE  DT_PATH(example_sensor)
#define LED0_NODE DT_PATH(blink_led)

#if DT_NODE_EXISTS(SW0_NODE)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, input_gpios, {0});
#endif

#if DT_NODE_EXISTS(LED0_NODE)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED0_NODE, led_gpios, {0});
#endif

static datalogger_ring_buf_t logger_buffer;

int main(void)
{
    printk("=== SAMD21 DataLogger with Ring Buffer ===\n");

    datalogger_buf_init(&logger_buffer);

    #if DT_NODE_EXISTS(SW0_NODE)
    if (button.port && device_is_ready(button.port)) {
        gpio_pin_configure_dt(&button, GPIO_INPUT);
        printk("[GPIO] Sensor IO1 (PA11) pronto.\n");
    }
    #endif

    #if DT_NODE_EXISTS(LED0_NODE)
    if (led.port && device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        printk("[GPIO] LED Status (PB30) pronto.\n");
    }
    #endif

    datalogger_entry_t new_sample;
    datalogger_entry_t pop_sample;
    char json_buf[128];

    while (1) {
        new_sample.timestamp_ms = k_uptime_get_32();

        #if DT_NODE_EXISTS(SW0_NODE)
        if (button.port && device_is_ready(button.port)) {
            new_sample.digital_state = (bool)gpio_pin_get_dt(&button);
            new_sample.raw_io_val = new_sample.digital_state ? 1023 : 0;
        } else
        #endif
        {
            new_sample.digital_state = !new_sample.digital_state;
            new_sample.raw_io_val = (new_sample.raw_io_val + 50) % 1024;
        }

        /* 1. Armazena a leitura no Ring Buffer */
        datalogger_buf_push(&logger_buffer, &new_sample);

        #if DT_NODE_EXISTS(LED0_NODE)
        if (led.port && device_is_ready(led.port)) {
            gpio_pin_toggle_dt(&led);
        }
        #endif

        /* 2. Processa/Despacha do Ring Buffer para a saida JSON/Ethernet */
        if (datalogger_buf_pop(&logger_buffer, &pop_sample) == 0) {
            if (datalogger_format_json(&pop_sample, json_buf, sizeof(json_buf)) == 0) {
                printk("[DataLogger RingBuf (%u in queue)] %s\n",
                       (unsigned int)datalogger_buf_count(&logger_buffer),
                       json_buf);
            }
        }

        k_sleep(K_MSEC(2000));
    }

    return 0;
}
