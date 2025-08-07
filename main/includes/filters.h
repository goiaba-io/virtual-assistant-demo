#pragma once

#include <stddef.h>
#include <stdint.h>

// General voice filters
int32_t high_pass_filter(int32_t input);
int32_t dc_block_filter(int32_t input);
void reset_voice_filters(void);

// Wind suppression filters
int32_t wind_highpass_filter(int32_t input);
int16_t limit_amplitude(int32_t sample);

// Noise gate filter
void noise_gate_filter(int32_t *samples, size_t count);
void set_noise_gate_threshold(int32_t threshold);
void set_noise_gate_attenuation(float factor);
