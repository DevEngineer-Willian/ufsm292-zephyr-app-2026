#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/ieee802154_radio.h>

/* Importação corrigida apontando para a pasta include mapeada pelo Zephyr */
#include <app/lib/sensor_frame.h> 

#define I2C_NODE DT_NODELABEL(sercom1)
#define BNO055_ADDR 0x29
#define TEMP_SENSOR_ADDR 0x4f
#define TEMP_REGISTER 0x00
#define ADC_NODE DT_ALIAS(light_adc)

#define REG_OPR_MODE 0x3D
#define REG_ACCEL_DATA_X_LSB 0x08
#define MODE_ACCONLY 0x01
#define ADC_CHANNEL_ID 6U

int main(void) {
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
    struct adc_channel_cfg channel_cfg = {
        .channel_id = ADC_CHANNEL_ID,
        .gain = ADC_GAIN_1_2,
        .reference = ADC_REF_VDD_1_2,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    };
    
    uint16_t light_raw = 0;
    struct adc_sequence sequence = {
        .buffer = &light_raw,
        .buffer_size = sizeof(light_raw),
        .resolution = 12,
        .oversampling = 0,
        .channels = BIT(ADC_CHANNEL_ID),
    };
    
    uint8_t chip_id = 0;
    uint8_t accel_bytes[6];
    uint8_t temp_bytes[2];
    int32_t light_mv;
    int ret;

    struct sensor_reading tx_packet = {
        .node_id = 1,
        .seq = 0,
        .flags = 0
    };

    if (!device_is_ready(i2c_dev)) {
        printk("Erro: I2C nao pronto.\n");
        return 0;
    }

    if (!device_is_ready(adc_dev)) {
        printk("Erro: ADC nao pronto.\n");
        return 0;
    }

    ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret < 0) {
        printk("Erro ao configurar canal ADC (%d).\n", ret);
        return 0;
    }

    i2c_reg_read_byte(i2c_dev, BNO055_ADDR, 0x00, &chip_id);
    printk("==> CHIP ID LIDO DO SENSOR: 0x%X\n", chip_id);

    i2c_reg_write_byte(i2c_dev, BNO055_ADDR, REG_OPR_MODE, MODE_ACCONLY);
    k_msleep(50);

    const struct device *const radio = DEVICE_DT_GET(DT_CHOSEN(zephyr_ieee802154));
    if (!device_is_ready(radio)) {
        printk("Erro: Dispositivo de radio nao pronto.\n");
        return 0;
    }
    
    const struct ieee802154_radio_api *api = (const struct ieee802154_radio_api *)radio->api;
    api->set_channel(radio, 15);
    api->set_txpower(radio, 0);
    api->start(radio);
    
    struct sensor_frame_cfg cfg = {
        .pan_id = 0xCAFE,
        .dst_short_addr = 0xFFFF, 
        .src_short_addr = 1,      
        .mac_seq = 0,
        .ack_request = false
    };
    
    uint8_t psdu[SENSOR_FRAME_LEN]; 

    printk("==> Radio Nativo Pronto! API Direta configurada.\n");

    while (1) {
        ret = i2c_burst_read(i2c_dev, BNO055_ADDR, REG_ACCEL_DATA_X_LSB, accel_bytes, 6);
        if (ret == 0) {
            tx_packet.accel[0] = (int16_t)((accel_bytes[1] << 8) | accel_bytes[0]);
            tx_packet.accel[1] = (int16_t)((accel_bytes[3] << 8) | accel_bytes[2]);
            tx_packet.accel[2] = (int16_t)((accel_bytes[5] << 8) | accel_bytes[4]);
        }

        ret = i2c_burst_read(i2c_dev, TEMP_SENSOR_ADDR, TEMP_REGISTER,
                             temp_bytes, sizeof(temp_bytes));
        if (ret == 0) {
            int16_t temp_raw = (int16_t)((temp_bytes[0] << 8) | temp_bytes[1]);
            int16_t temp_real = temp_raw >> 4;
            tx_packet.temp_c_x100 = (int16_t)((temp_real * 625) / 100); 
        }

        ret = adc_read(adc_dev, &sequence);
        if (ret == 0) {
            light_mv = (int32_t)light_raw;
            ret = adc_raw_to_millivolts(1650, ADC_GAIN_1_2, 12, &light_mv);
            if (ret == 0) {
                tx_packet.light = (uint16_t)light_mv;
            }
        }

        tx_packet.uptime_ms = k_uptime_get_32();
        tx_packet.seq++;

        printk("Montado [%d] -> Luz:%u Temp:%d.%02dC X:%d Y:%d Z:%d\n",
               tx_packet.seq, tx_packet.light, 
               tx_packet.temp_c_x100 / 100, 
               tx_packet.temp_c_x100 >= 0 ? (tx_packet.temp_c_x100 % 100) : -(tx_packet.temp_c_x100 % 100), 
               tx_packet.accel[0], tx_packet.accel[1], tx_packet.accel[2]);

        int enc_ret = sensor_frame_encode(psdu, sizeof(psdu), &cfg, &tx_packet);
        
        if (enc_ret >= 0) {
            struct net_pkt *pkt = net_pkt_alloc_with_buffer(NULL, SENSOR_FRAME_LEN,
                                                            NET_AF_UNSPEC, 0, K_NO_WAIT);
            if (pkt != NULL) {
                if (net_pkt_write(pkt, psdu, SENSOR_FRAME_LEN) == 0) {
                    struct net_buf *frag = net_buf_frag_last(pkt->buffer);
                    api->tx(radio, IEEE802154_TX_MODE_CSMA_CA, pkt, frag);
                    printk("--> %d bytes formatados e enviados pelo ar!\n", SENSOR_FRAME_LEN);
                }
                net_pkt_unref(pkt);
            }
            cfg.mac_seq++; 
        } else {
            printk("ERRO: O codec falhou ao forjar o pacote! (Codigo: %d)\n", enc_ret);
        }
        
        k_msleep(1000);
    }
    return 0;
}