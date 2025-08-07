#include "speaker.h"

#include "driver/i2s.h"
#include "utils.h"

#define I2S_SPK_PORT I2S_NUM_1

void spk_begin(void) {
    i2s_config_t cfg = {.mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_cfg = {.bck_io_num = SPK_BCLK,
        .ws_io_num = SPK_WS,
        .data_out_num = SPK_DATA,
        .data_in_num = I2S_PIN_NO_CHANGE};

    i2s_driver_install(I2S_SPK_PORT, &cfg, 0, NULL);
    i2s_set_pin(I2S_SPK_PORT, &pin_cfg);
}

void spk_play(int16_t* src, size_t len_samples) {
    size_t idx = 0;
    while (idx < len_samples) {
        size_t chunk = (len_samples - idx > 256) ? 256 : (len_samples - idx);
        size_t written = 0;
        i2s_write(I2S_SPK_PORT,
            src + idx,
            chunk * sizeof(int16_t),
            &written,
            portMAX_DELAY);
        idx += written / sizeof(int16_t);
    }
}

esp_err_t spk_write(int16_t* buf, size_t size, size_t* bytes_written) {
    return i2s_write(I2S_SPK_PORT,
        buf,
        size * sizeof(int16_t),
        bytes_written,
        portMAX_DELAY);
}
