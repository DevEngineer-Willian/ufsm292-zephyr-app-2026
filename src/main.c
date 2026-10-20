#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <app/drivers/blink.h>

/* Obtém o dispositivo configurado com "compatible = blink-gpio-led" */
static const struct device *const blink_dev = DEVICE_DT_GET(DT_NODELABEL(blink_led));

int main(void)
{
    int ret;

    printk("\n=============================================\n");
    printk("   ZEPHYR RTOS - DRIVER NATIVO BLINK_GPIO_LED \n");
    printk("=============================================\n");

    /* 1. Validação de Prontidão do Dispositivo */
    if (!device_is_ready(blink_dev)) {
        printk("ERRO: Dispositivo de Blink customizado nao esta pronto!\n");
        return -ENODEV;
    }

    printk("Dispositivo de Blink customizado reconhecido pelo Kernel.\n");

    /* 2. Configura o período de piscada usando a API oficial do driver */
    ret = blink_set_period_ms(blink_dev, 500);
    if (ret < 0) {
        printk("ERRO (%d): Falha ao definir o periodo do LED!\n", ret);
        return ret;
    }

    printk("Periodo definido para 500ms via blink_set_period_ms().\n");

    /* 3. Loop principal monitorando o funcionamento */
    while (1) {
        printk("LOG: Driver Blink operando normalmente...\n");
        k_msleep(2000);
    }

    return 0;
}