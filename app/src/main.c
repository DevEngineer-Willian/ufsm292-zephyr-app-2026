/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <datalogger/datalogger.h>

/* Resolução dos nós via DT_PATH para o overlay da SAMD21 */
#define SW0_NODE  DT_PATH(example_sensor)
#define LED0_NODE DT_PATH(blink_led)

#if DT_NODE_EXISTS(SW0_NODE)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, input_gpios, {0});
#endif

#if DT_NODE_EXISTS(LED0_NODE)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(LED0_NODE, led_gpios, {0});
#endif

int main(void)
{
    printk("=== SAMD21 DataLogger Hardware Integration ===\n");

    datalogger_init();

    /* Configuração do pino de entrada (IO1) */
    #if DT_NODE_EXISTS(SW0_NODE)
    if (button.port && device_is_ready(button.port)) {
        gpio_pin_configure_dt(&button, GPIO_INPUT);
        printk("[GPIO] Sensor IO1 (PA11) inicializado.\n");
    }
    #endif

    /* Configuração do pino de saída (LED Status) */
    #if DT_NODE_EXISTS(LED0_NODE)
    if (led.port && device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        printk("[GPIO] LED Status (PB30) inicializado.\n");
    }
    #endif

    datalogger_entry_t sample;
    char json_buf[128];

    while (1) {
        sample.timestamp_ms = k_uptime_get_32();

        /* Leitura real do pino digital IO1 */
        #if DT_NODE_EXISTS(SW0_NODE)
        if (button.port && device_is_ready(button.port)) {
            sample.digital_state = (bool)gpio_pin_get_dt(&button);
            sample.raw_io_val = sample.digital_state ? 1023 : 0;
        } else
        #endif
        {
            /* Fallback de simulação caso o pino não esteja acessível */
            sample.digital_state = !sample.digital_state;
            sample.raw_io_val = (sample.raw_io_val + 50) % 1024;
        }

        /* Alterna o estado do LED a cada leitura */
        #if DT_NODE_EXISTS(LED0_NODE)
        if (led.port && device_is_ready(led.port)) {
            gpio_pin_toggle_dt(&led);
        }
        #endif

        if (datalogger_format_json(&sample, json_buf, sizeof(json_buf)) == 0) {
            printk("[DataLogger] HW JSON: %s\n", json_buf);
        } else {
            printk("[DataLogger] Erro ao formatar payload!\n");
        }

        k_sleep(K_MSEC(2000));
    }

    return 0;
}
