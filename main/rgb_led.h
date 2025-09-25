#ifndef RGB_LED_C_H
#define RGB_LED_C_H

#include <stdint.h>
#include <stdbool.h>

// Em C, usamos uma struct para agrupar todos os dados.
typedef struct {
    void* strip_handle; // Ponteiro opaco para o handle do led_strip

    // Estado alvo do LED
    uint8_t targetR, targetG, targetB;

    // Estado de controle
    bool isOn;
    bool isPulsing;
    float pulsesPerSecond;
    int transitionMs;

    // Variáveis para o controle do pulso
    int64_t lastUpdateTime;
    int64_t cycleDuration;
    bool isFadingIn;
} RgbLed;


#ifdef __cplusplus
extern "C" {
#endif

// Função de inicialização
void rgb_led_init(RgbLed* led, int pixelPin);

// Função de controle principal
void rgb_led_onled(RgbLed* led, const char* state, const char* color, float pulsesPerSecond, int transition);

// Função de atualização (deve ser chamada em um loop)
void rgb_led_update(RgbLed* led);

// Função para liberar recursos
void rgb_led_deinit(RgbLed* led);
void led_status_off(RgbLed* led);
void led_status_initializing(RgbLed* led);
void led_status_wifi_error(RgbLed* led);
void led_status_ready(RgbLed* led);
void led_status_thinking(RgbLed* led);
void led_status_responding(RgbLed* led);
void led_status_server_error(RgbLed* led);


#ifdef __cplusplus
}
#endif

#endif // RGB_LED_C_H