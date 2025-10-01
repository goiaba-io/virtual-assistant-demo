#include "audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mic.h"
#include "speaker.h"
#include "utils.h"
#include "webrtc.h"
#include "wifi.h"
#include "rgb_led.h"
#include "driver/gpio.h" 

#define NEOPIXEL_PIN GPIO_NUM_48 // Pin  LED RGB ESP32-S3

RgbLed status_led; 
void led_update_task(void *pvParameters);

void app_main(void) {
    rgb_led_init(&status_led, NEOPIXEL_PIN);
    rgb_led_onled(&status_led, "on", "blue", 0, 0);
    xTaskCreate(led_update_task, "led_update_task", 2048, NULL, 5, NULL);
    wifi_init(CONFIG_WIFI_CONNECT_SSID, CONFIG_WIFI_CONNECT_PASSWORD);
    vTaskDelay(pdMS_TO_TICKS(5000));
    mic_begin();
    spk_begin();
    init_audio_decoder();
    webrtc_init();
    webrtc_register_connection_task();
    webrtc_register_send_audio_task();
    vTaskDelete(NULL);
}

void led_update_task(void *pvParameters) {
    while (1) {
        rgb_led_update(&status_led);
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}