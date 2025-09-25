
#include "webrtc.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "audio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "filters.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http.h"
#include "mic.h"
#include "peer.h"
#include "speaker.h"
#include "utils.h"
#include "rgb_led.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h" 
#include "driver/gpio.h"

extern RgbLed status_led;
static TimerHandle_t inactivity_timer = NULL; // Variável para o nosso timer

//------------ Buton -----------------
#define MUTE_BUTTON_GPIO GPIO_NUM_39
static volatile bool g_is_muted = false;
static bool g_last_mute_state = false;
static bool g_is_thinking = false; 

static const char *TAG = "webrtc";
#define READ_BUFFER_SAMPLES FRAME_SAMPLES
#define READ_BUFFER_SIZE_BYTES (READ_BUFFER_SAMPLES * sizeof(int16_t))

#define GREETING_JSON                                                 \
    "{\"type\": \"response.create\", \"response\": {\"modalities\": " \
    "[\"audio\", \"text\"], \"instructions\": \"" GREETING "\"}}"

#define INSTRUCTION_JSON                                              \
    "{\"type\": \"session.update\", \"session\": {\"instructions\": " \
    "\"" INSTRUCTIONS "\"}}"


StaticTask_t task_buffer;
PeerConnection *g_pc = NULL;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

int64_t get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL + tv.tv_usec / 1000LL);
}


static void inactivity_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Inatividade detectada. Voltando ao estado 'Pronto'.");
    led_status_ready(&status_led);
}


static void oniceconnectionstatechange(PeerConnectionState state,
    void *user_data) {
    ESP_LOGI(TAG, "PeerConnectionState: %d", state);
    eState = state;
    if (state != PEER_CONNECTION_COMPLETED) {
        gDataChannelOpened = 0;
    }
}

static void onmessage(char *msg, size_t len, void *userdata, uint16_t sid) {
    ESP_LOGI(TAG, "Datachannel message: %.*s", len, msg);
    if (inactivity_timer != NULL) {
        xTimerReset(inactivity_timer, 0);
    }
    if (strstr(msg, "\"type\":\"input_audio") != NULL) {
        led_status_thinking(&status_led);
    } else if (strstr(msg, "\"type\":\"output_audio_buffer.stopped\"") != NULL) {
        led_status_ready(&status_led);
        if (inactivity_timer != NULL) {
            xTimerStop(inactivity_timer, 0);
        }
    } else if (strstr(msg, "\"type\":\"output_audio") != NULL) {
        led_status_responding(&status_led);
    }
}

static void onopen(void *userdata) {
    ESP_LOGI(TAG, "Datachannel opened");
    gDataChannelOpened = 1;
    if (peer_connection_create_datachannel(g_pc,
            DATA_CHANNEL_RELIABLE,
            0,
            0,
            (char *)"oai-events",
            (char *)"") != -1) {
        ESP_LOGI(TAG, "DataChannel created");
        peer_connection_datachannel_send(g_pc,
            (char *)GREETING_JSON,
            strlen(GREETING_JSON));
        peer_connection_datachannel_send(g_pc,
            (char *)INSTRUCTION_JSON,
            strlen(INSTRUCTION_JSON));
    } else {
        ESP_LOGE(TAG, "Failed to create DataChannel");
    }
}

static void connection_task(void *arg) {
    ESP_LOGI(TAG, "Connection task started");
    peer_connection_create_offer(g_pc);

    for (;;) {
        peer_connection_loop(g_pc);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

static int send_audio(const uint8_t *buf, size_t size) {
    if (gDataChannelOpened) {
        peer_connection_send_audio(g_pc, buf, size);
    }
    return 0;
}

static void send_audio_task(void *arg) {
    ESP_LOGI(TAG, "Audio send task started");

    int32_t raw_buffer[READ_BUFFER_SAMPLES];
    int16_t filtered_buffer[FRAME_SAMPLES];
    const float mic_gain = 0.025f;
    
    const int32_t SPEECH_THRESHOLD = 50000;

    init_audio_encoder();
    reset_voice_filters();

    for (;;) {
        // Bloco de debug para o botão (continua igual)
        if (g_is_muted != g_last_mute_state) {
            if (g_is_muted) {
                ESP_LOGW(TAG, "Botão Pressionado: Microfone MUTADO");
            } else {
                ESP_LOGI(TAG, "Botão Pressionado: Microfone DESMUTADO");
            }
            g_last_mute_state = g_is_muted;
        }

        // --- LÓGICA DE MUDO CORRIGIDA E MAIS ROBUSTA ---
        if (g_is_muted) {
            // Se estiver mutado:
            // 1. Define o LED para o estado "mutado"
            led_status_muted(&status_led);

            // 2. Prepara um buffer contendo apenas silêncio (zeros)
            int16_t silent_buffer[FRAME_SAMPLES] = {0};
            
            // 3. Codifica e envia o buffer de silêncio. Isso mantém a conexão de áudio ativa
            //    mas garante que nenhum som do microfone seja transmitido.
            audio_encode(silent_buffer, FRAME_SAMPLES, send_audio);

        } else {
            // Se NÃO estiver mutado, executa a lógica normal de captura de áudio:
            size_t samples = mic_read(raw_buffer, READ_BUFFER_SAMPLES);
            noise_gate_filter(raw_buffer, samples);
            
            int32_t energy = 0;
            for (size_t i = 0; i < samples; i++) {
                energy += abs(raw_buffer[i]);
            }

            if (energy > SPEECH_THRESHOLD) {
                if (!g_is_thinking) {
                    g_is_thinking = true;
                    led_status_thinking(&status_led);
                }
            } else {
                if (g_is_thinking) {
                    g_is_thinking = false;
                    led_status_ready(&status_led);
                }
            }

            for (size_t i = 0; i < samples; i++) {
                int32_t sample = raw_buffer[i];
                sample = dc_block_filter(sample);
                sample = high_pass_filter(sample);
                filtered_buffer[i] =
                    limit_amplitude((int32_t)(sample * mic_gain) >> 11);
            }

            audio_encode(filtered_buffer, samples, send_audio);
        }

        // O delay é executado em ambos os casos para manter o ritmo da tarefa
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void on_audio_track_cb(uint8_t *data, size_t size, void *userdata) {
    audio_decode(data, size, spk_write);
}

static void on_icecandidate_task(char *description, void *user_data) {
    char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    openai_http_request(description, local_buffer);
    peer_connection_set_remote_description(g_pc, local_buffer, SDP_TYPE_ANSWER);
}

void webrtc_init(const char *ssid, const char *password) {
    PeerConfiguration config = {
        .ice_servers = {{.urls = "stun:stun.l.google.com:19302"}},
        .datachannel = DATA_CHANNEL_STRING,
        .audio_codec = CODEC_OPUS,
        .video_codec = CODEC_NONE,
        .onaudiotrack = on_audio_track_cb,
        .onvideotrack = NULL,
        .on_request_keyframe = NULL,
        .user_data = NULL,
    };
    ESP_LOGI(TAG,
        "Initializing libpeer with ICE server: %s",
        config.ice_servers[0].urls);

    peer_init();

    inactivity_timer = xTimerCreate(
        "InactivityTimer",          // Nome para debug
        pdMS_TO_TICKS(5000),        // Período de 5000ms (5 segundos)
        pdFALSE,                    // Não recarregar automaticamente (one-shot)
        (void *)0,                  // ID do timer (não usado)
        inactivity_timer_callback   // Função a ser chamada no timeout
    );

 
    g_pc = peer_connection_create(&config);
    if (!g_pc) {
        ESP_LOGE(TAG, "peer_connection_create failed");
        led_status_server_error(&status_led);
        return;
    }

    peer_connection_oniceconnectionstatechange(g_pc,
        oniceconnectionstatechange);
    peer_connection_onicecandidate(g_pc, on_icecandidate_task);
    peer_connection_ondatachannel(g_pc, onmessage, onopen, NULL);

    // vTaskDelay(pdMS_TO_TICKS(5000));
    led_status_ready(&status_led);    
    ESP_LOGI(TAG, "Peer manager initialized");
}

void webrtc_register_connection_task(void) {
    if (g_pc == NULL) {
        ESP_LOGE(TAG, "PeerConnection not initialized");
        led_status_server_error(&status_led);
        return;
    }

    if (xTaskCreate(connection_task, "conn", 16 * 1024, NULL, 5, NULL) !=
        pdPASS) {
        ESP_LOGW(TAG, "Failed to create connection task");
        led_status_server_error(&status_led);
    }
}

void webrtc_register_send_audio_task(void) {
    if (g_pc == NULL) {
        ESP_LOGE(TAG, "PeerConnection not initialized");
        led_status_server_error(&status_led);
        return;
    }

    if (xTaskCreate(send_audio_task, "send_audio", 32 * 1024, NULL, 5, NULL) !=
        pdPASS) {
        ESP_LOGW(TAG, "Failed to create send_audio task");
        led_status_server_error(&status_led);
    }
}
//------------- Buton -------------

void IRAM_ATTR mute_button_isr_handler(void* arg) {
    // Apenas inverte o estado da flag. Nada mais.
    g_is_muted = !g_is_muted;
}

void button_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Gatilho na borda de descida (pressionar)
        .pin_bit_mask = (1ULL << MUTE_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Habilita resistor de pull-up interno
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&io_conf);

    // Instala o serviço de ISR de GPIO
    gpio_install_isr_service(0);
    // Adiciona nossa função de handler para o pino específico
    gpio_isr_handler_add(MUTE_BUTTON_GPIO, mute_button_isr_handler, NULL);

    ESP_LOGI(TAG, "Botão de Mudo configurado no GPIO %d", MUTE_BUTTON_GPIO);
}

//--------------------------