#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
	int ret;
	bool led_ligado = true;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	printk("LED ligado\r\n");

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			printk("Falha ao alternar o LED (%d)\r\n", ret);
			return 0;
		}

		led_ligado = !led_ligado;
		printk("LED %s\r\n", led_ligado ? "ligado" : "desligado");
		k_msleep(500);
	}

	return 0;
}
