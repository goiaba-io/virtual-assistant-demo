#include "filters.h"

#include <math.h>
#include <stdlib.h>

// === High-pass filter (around 300 Hz) ===
static float hpf_prev_input = 0.0f;
static float hpf_prev_output = 0.0f;

#define HPF_ALPHA 0.965f  // Tune for ~300 Hz at 16kHz

int32_t high_pass_filter(int32_t input) {
    float in = (float)input;
    float out = HPF_ALPHA * (hpf_prev_output + in - hpf_prev_input);
    hpf_prev_input = in;
    hpf_prev_output = out;
    return (int32_t)out;
}

// === DC Block filter ===
static int32_t dc_prev_input = 0;
static int32_t dc_prev_output = 0;

int32_t dc_block_filter(int32_t input) {
    int32_t output = input - dc_prev_input + (dc_prev_output >> 1);
    dc_prev_input = input;
    dc_prev_output = output;
    return output;
}

// === Wind suppression: 2nd-order high-pass filter (e.g. 400 Hz) ===
// Coefficients for ~400 Hz HPF at 16kHz (Butterworth-style)
#define B0 0.9114f
#define B1 -1.8228f
#define B2 0.9114f
#define A1 -1.8227f
#define A2 0.8372f

static float wpf_x1 = 0, wpf_x2 = 0;
static float wpf_y1 = 0, wpf_y2 = 0;

int32_t wind_highpass_filter(int32_t input) {
    float x0 = (float)input;
    float y0 = B0 * x0 + B1 * wpf_x1 + B2 * wpf_x2 - A1 * wpf_y1 - A2 * wpf_y2;

    // Shift state
    wpf_x2 = wpf_x1;
    wpf_x1 = x0;
    wpf_y2 = wpf_y1;
    wpf_y1 = y0;

    return (int32_t)y0;
}

// === Amplitude limiter to suppress bursty wind pressure ===
#define LIMIT_MAX 28000
#define LIMIT_MIN -28000

int16_t limit_amplitude(int32_t sample) {
    if (sample > LIMIT_MAX) return LIMIT_MAX;
    if (sample < LIMIT_MIN) return LIMIT_MIN;
    return (int16_t)sample;
}

// === Optional: Reset filter states ===
void reset_voice_filters(void) {
    hpf_prev_input = hpf_prev_output = 0;
    dc_prev_input = dc_prev_output = 0;
    wpf_x1 = wpf_x2 = wpf_y1 = wpf_y2 = 0;
}

static int32_t noise_threshold = 500000;
static float attenuation_factor = 0.0f;

void set_noise_gate_threshold(int32_t threshold) {
    noise_threshold = threshold;
}

void set_noise_gate_attenuation(float factor) { attenuation_factor = factor; }

void noise_gate_filter(int32_t *samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int32_t sample = samples[i];
        if (abs(sample) < noise_threshold) {
            samples[i] = (int32_t)(sample * attenuation_factor);
        }
    }
}
