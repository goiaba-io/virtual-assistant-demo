#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void mic_begin(void);
size_t mic_record(int32_t *dst, size_t len_samples);
size_t mic_read(int32_t *dst, size_t len_samples);

#ifdef __cplusplus
}
#endif
