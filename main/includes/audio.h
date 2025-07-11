#pragma once
#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef esp_err_t (
    *audio_write_cb_t)(int16_t* buf, size_t size, size_t* bytes_written);
typedef int (*audio_send_cb_t)(const uint8_t* buf, size_t size);

void init_audio_encoder();
void init_audio_decoder();
void audio_decode(uint8_t* data, size_t size, audio_write_cb_t i2c_write_cb);
void audio_encode(int16_t* pcm_samples, size_t sample_count,
    audio_send_cb_t send_cb);
void apply_digital_gain(int16_t* pcm, size_t n, float gain);
#ifdef __cplusplus
}
#endif
