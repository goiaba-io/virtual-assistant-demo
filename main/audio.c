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

static opus_int16 *decode_output_buffer = NULL;
static uint8_t *encoder_output_buffer = NULL;

static OpusDecoder *opus_decoder = NULL;
static OpusEncoder *opus_encoder = NULL;

void init_audio_decoder() {
    int decoder_error = 0;
    opus_decoder = opus_decoder_create(OPUS_SAMPLE_RATE, 1, &decoder_error);
    if (decoder_error != OPUS_OK) {
        ESP_LOGE(TAG,
            "Failed to create OPUS decoder: %s",
            opus_strerror(decoder_error));
        return;
    }

    decode_output_buffer =
        (opus_int16 *)malloc(FRAME_SAMPLES * sizeof(opus_int16));
    if (!decode_output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate decoder output buffer");
    }
}

void audio_decode(uint8_t *data, size_t size, audio_write_cb_t i2s_write_cb) {
    if (!opus_decoder || !decode_output_buffer) return;

    int decoded_samples = opus_decode(opus_decoder,
        data,
        size,
        decode_output_buffer,
        FRAME_SAMPLES,
        0);

    if (decoded_samples > 0) {
        size_t bytes = 0;
        esp_err_t err =
            i2s_write_cb(decode_output_buffer, decoded_samples, &bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                "audio write callback failed: %s",
                esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG,
            "No decoded samples or decode error: %s",
            opus_strerror(decoded_samples));
    }
}

void init_audio_encoder() {
    int encoder_error;
    opus_encoder = opus_encoder_create(OPUS_SAMPLE_RATE,
        1,
        OPUS_APPLICATION_VOIP,
        &encoder_error);
    if (encoder_error != OPUS_OK) {
        ESP_LOGE(TAG,
            "Failed to create OPUS encoder: %s",
            opus_strerror(encoder_error));
        return;
    }

    opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
    opus_encoder_ctl(opus_encoder,
        OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
    opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    encoder_output_buffer = (uint8_t *)malloc(OPUS_OUT_BUFFER_SIZE);
    if (!encoder_output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate encoder output buffer");
    }
}

void audio_encode(int16_t *pcm_samples, size_t sample_count,
    audio_send_cb_t send_cb) {
    if (!opus_encoder || !encoder_output_buffer) return;

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
