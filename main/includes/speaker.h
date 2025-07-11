#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void spk_begin(void);
void spk_play(int16_t* src, size_t len_samples);
esp_err_t spk_write(int16_t* buf, size_t size, size_t* bytes_written);

#ifdef __cplusplus
}
#endif
