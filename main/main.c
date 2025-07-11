#include "audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mic.h"
#include "speaker.h"
#include "utils.h"
#include "webrtc.h"
#include "wifi.h"

void app_main(void) {
    wifi_init(CONFIG_WIFI_CONNECT_SSID, CONFIG_WIFI_CONNECT_PASSWORD);
    mic_begin();
    spk_begin();
    init_audio_decoder();
    webrtc_init();
    webrtc_register_connection_task();
    webrtc_register_send_audio_task();
    vTaskDelete(NULL);
}
