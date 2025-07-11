#include "audio.h"

#include <opus.h>
#include <stdatomic.h>
#include <string.h>

#include "esp_log.h"

#define OPUS_SAMPLE_RATE 16000
#define FRAME_SAMPLES (OPUS_SAMPLE_RATE / 50)

#define OPUS_OUT_BUFFER_SIZE 1276

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

#define READ_BUFFER_SIZE (2560)
#define RESAMPLE_BUFFER_SIZE (FRAME_SAMPLES * sizeof(opus_int16))

static int16_t decode_resample_buffer[READ_BUFFER_SIZE];

static const char *TAG = "audio";
static atomic_bool is_playing = ATOMIC_VAR_INIT(false);

#define ENERGY_THRESHOLD 500000UL  // ajuste conforme seu ruído de fundo

static void update_is_playing(const opus_int16 *buf, size_t samples) {
    uint64_t sum_sq = 0;
    for (size_t i = 0; i < samples; i++) {
        int32_t v = buf[i];
        sum_sq += (uint64_t)(v * v);
    }
    uint32_t avg_sq = (uint32_t)(sum_sq / samples);

    if (avg_sq > ENERGY_THRESHOLD) {
        atomic_store(&is_playing, true);
    } else {
        atomic_store(&is_playing, false);
    }
}

void convert_16k8_to_32k32(int16_t *in_buf, size_t in_samples,
    int32_t *out_buf) {
    size_t out_index = 0;
    // bool any_set = false;
    for (size_t i = 0; i < in_samples; i++) {
        // if (in_buf[i] != -1 && in_buf[i] != 0) {
        //     any_set = true;
        // }

        int32_t s = ((int32_t)in_buf[i]) << 16;
        out_buf[out_index++] = s;
        out_buf[out_index++] = s;
        out_buf[out_index++] = s;
        out_buf[out_index++] = s;
    }
    // is_playing = any_set;
}

static opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;

void init_audio_decoder() {
    int decoder_error = 0;
    opus_decoder = opus_decoder_create(OPUS_SAMPLE_RATE, 1, &decoder_error);
    if (decoder_error != OPUS_OK) {
        printf("Failed to create OPUS decoder");
        return;
    }

    output_buffer = (opus_int16 *)malloc(RESAMPLE_BUFFER_SIZE);
}

void audio_decode(uint8_t *data, size_t size, audio_write_cb_t i2c_write_cb) {
    int decoded_samples = opus_decode(opus_decoder,
        data,
        size,
        output_buffer,
        RESAMPLE_BUFFER_SIZE,
        0);

    if (decoded_samples > 0) {
        update_is_playing(output_buffer, decoded_samples);

        convert_16k8_to_32k32((int16_t *)output_buffer,
            RESAMPLE_BUFFER_SIZE / sizeof(int16_t),
            (int32_t *)decode_resample_buffer);

        size_t bytes;
        esp_err_t err =
            i2c_write_cb((int16_t *)output_buffer, decoded_samples, &bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                "audio write callback failed: %s",
                esp_err_to_name(err));
        }
    }
}

OpusEncoder *opus_encoder = NULL;
static uint8_t *encoder_output_buffer = NULL;

void init_audio_encoder() {
    int encoder_error;
    opus_encoder = opus_encoder_create(OPUS_SAMPLE_RATE,
        1,
        OPUS_APPLICATION_VOIP,
        &encoder_error);
    if (encoder_error != OPUS_OK) {
        printf("Failed to create OPUS encoder");
        return;
    }

    if (opus_encoder_init(opus_encoder,
            OPUS_SAMPLE_RATE,
            1,
            OPUS_APPLICATION_VOIP) != OPUS_OK) {
        printf("Failed to initialize OPUS encoder");
        return;
    }

    opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
    opus_encoder_ctl(opus_encoder,
        OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
    opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    encoder_output_buffer = malloc(OPUS_OUT_BUFFER_SIZE);
}

void audio_encode(int16_t *pcm_samples, size_t sample_count,
    audio_send_cb_t send_cb) {
    if (is_playing) {
        ESP_LOGW(TAG, "Audio encoder is already playing");
        memset(pcm_samples, 0, sample_count * sizeof(int16_t));
    }

    int nb_bytes = opus_encode(opus_encoder,
        pcm_samples,
        sample_count,
        encoder_output_buffer,
        OPUS_OUT_BUFFER_SIZE);
    if (nb_bytes < 0) {
        ESP_LOGE(TAG, "Opus encoding failed: %s", opus_strerror(nb_bytes));
        return;
    }
    send_cb(encoder_output_buffer, nb_bytes);
}

void apply_digital_gain(int16_t *pcm, size_t n, float gain) {
    for (size_t i = 0; i < n; i++) {
        int32_t v = (int32_t)(pcm[i] * gain);
        if (v > INT16_MAX)
            v = INT16_MAX;
        else if (v < INT16_MIN)
            v = INT16_MIN;
        pcm[i] = (int16_t)v;
    }
}
