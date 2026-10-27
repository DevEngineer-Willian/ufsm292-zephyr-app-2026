/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/socket.h>
#include <datalogger/datalogger.h>

#define SERVER_PORT 5000
#define SERVER_ADDR "192.168.1.100" /* Ajuste para o IP do seu servidor local */

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
    int sock;
    struct sockaddr_in server_addr;

    printk("=== SAMD21 DataLogger UDP Socket ===\n");

    datalogger_buf_init(&logger_buffer);

    #if DT_NODE_EXISTS(SW0_NODE)
    if (button.port && device_is_ready(button.port)) {
        gpio_pin_configure_dt(&button, GPIO_INPUT);
        printk("[GPIO] Sensor PA11 pronto.\n");
    }
    #endif

    #if DT_NODE_EXISTS(LED0_NODE)
    if (led.port && device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        printk("[GPIO] LED PB30 pronto.\n");
    }
    #endif

    /* Configuração do Socket UDP */
    sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printk("[UDP] Erro ao criar socket: %d\n", sock);
    } else {
        printk("[UDP] Socket criado com sucesso.\n");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    zsock_inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

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

        /* 2. Processa o buffer e envia via Socket UDP */
        if (datalogger_buf_pop(&logger_buffer, &pop_sample) == 0) {
            if (datalogger_format_json(&pop_sample, json_buf, sizeof(json_buf)) == 0) {
                printk("[DataLogger JSON] %s\n", json_buf);

                if (sock >= 0) {
                    int ret = zsock_sendto(sock, json_buf, strlen(json_buf), 0,
                                           (struct sockaddr *)&server_addr,
                                           sizeof(server_addr));
                    if (ret < 0) {
                        printk("[UDP] Falha no envio sendto: %d\n", ret);
                    } else {
                        printk("[UDP] %d bytes enviados para %s:%d\n", ret, SERVER_ADDR, SERVER_PORT);
                    }
                }
            }
        }

        k_sleep(K_MSEC(2000));
    }

    if (sock >= 0) {
        zsock_close(sock);
    }

    return 0;
}
