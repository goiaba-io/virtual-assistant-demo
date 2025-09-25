#include "rgb_led.h"
#include "led_strip.h" // <-- Nova biblioteca
#include "driver/gpio.h"
#include "esp_timer.h"
#include <string.h>

const float BLINK_SLOW = 1.0f; // 1 pulso por segundo
const float BLINK_FAST = 4.0f; // 4 pulsos por segundo
const int FADE_MS = 150;       // Transição suave em ms

// --- Funções Privadas (static) ---
static void _set_color_target(RgbLed* led, const char* color) {
    if (strcmp(color, "red") == 0) { led->targetR = 255; led->targetG = 0;   led->targetB = 0; }
    else if (strcmp(color, "green") == 0) { led->targetR = 0;   led->targetG = 255; led->targetB = 0; }
    else if (strcmp(color, "blue") == 0) { led->targetR = 0;   led->targetG = 0;   led->targetB = 255; }
    else if (strcmp(color, "yellow") == 0) { led->targetR = 255; led->targetG = 255; led->targetB = 0; }
    else if (strcmp(color, "cyan") == 0) { led->targetR = 0;   led->targetG = 255; led->targetB = 255; }
    else if (strcmp(color, "magenta") == 0) { led->targetR = 255; led->targetG = 0;   led->targetB = 255; }
    else if (strcmp(color, "white") == 0) { led->targetR = 255; led->targetG = 255; led->targetB = 255; }
    else { led->targetR = 0; led->targetG = 0; led->targetB = 0; }
}

static void _write_color(RgbLed* led, uint8_t r, uint8_t g, uint8_t b) {
    led_strip_handle_t strip = (led_strip_handle_t)led->strip_handle;
    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip);
}

// --- Funções Públicas (com linkage C) ---
extern "C" {

void rgb_led_init(RgbLed* led, int pixelPin) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = (gpio_num_t)pixelPin,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, (led_strip_handle_t*)&led->strip_handle);

    // Inicializa o estado
    rgb_led_onled(led, "off", "black", 0, 0);
}

void rgb_led_onled(RgbLed* led, const char* state, const char* color, float pulsesPerSecond, int transition) {
    if (strcmp(state, "off") == 0) {
        led->isOn = false;
        led->isPulsing = false;
        _write_color(led, 0, 0, 0);
        return;
    }
    
    led->isOn = true;
    _set_color_target(led, color);
    led->pulsesPerSecond = pulsesPerSecond;
    led->transitionMs = transition;

    if (led->pulsesPerSecond > 0) {
        led->isPulsing = true;
        led->cycleDuration = (int64_t)(1000.0 / led->pulsesPerSecond);
        if (led->transitionMs * 2 > led->cycleDuration) {
            led->transitionMs = led->cycleDuration / 2;
        }
        led->lastUpdateTime = esp_timer_get_time() / 1000;
        led->isFadingIn = true;
    } else {
        led->isPulsing = false;
        _write_color(led, led->targetR, led->targetG, led->targetB);
    }
}

void rgb_led_update(RgbLed* led) {
    if (!led->isOn || !led->isPulsing) {
        return;
    }

    int64_t currentTime = esp_timer_get_time() / 1000; // Tempo em milissegundos
    int64_t elapsedTime = currentTime - led->lastUpdateTime;
    
    int64_t fadeDuration = led->transitionMs;
    int64_t holdDuration = (led->cycleDuration / 2) - fadeDuration;
    if(holdDuration < 0) holdDuration = 0;

    float brightness_percentage = 0.0f;

    if (led->isFadingIn) {
        if (elapsedTime < fadeDuration) {
            brightness_percentage = (float)elapsedTime / fadeDuration;
        } else {
            brightness_percentage = 1.0f;
            if (elapsedTime >= (fadeDuration + holdDuration)) {
                led->isFadingIn = false;
                led->lastUpdateTime = currentTime;
            }
        }
    } else { // Fading out
        if (elapsedTime < fadeDuration) {
            brightness_percentage = 1.0f - ((float)elapsedTime / fadeDuration);
        } else {
            brightness_percentage = 0.0f;
            if (elapsedTime >= (fadeDuration + holdDuration)) {
                led->isFadingIn = true;
                led->lastUpdateTime = currentTime;
            }
        }
    }
    
    _write_color(
        led,
        (uint8_t)(led->targetR * brightness_percentage),
        (uint8_t)(led->targetG * brightness_percentage),
        (uint8_t)(led->targetB * brightness_percentage)
    );
}

void rgb_led_deinit(RgbLed* led) {
    if (led->strip_handle != NULL) {
        led_strip_del((led_strip_handle_t)led->strip_handle);
        led->strip_handle = NULL;
    }
}

void led_status_off(RgbLed* led) {
    rgb_led_onled(led, "off", "black", 0, 0);
}

void led_status_initializing(RgbLed* led) {
    // Amarelo, piscando lentamente
    rgb_led_onled(led, "on", "yellow", BLINK_SLOW, FADE_MS);
}

void led_status_wifi_error(RgbLed* led) {
    // Vermelho, piscando lentamente
    rgb_led_onled(led, "on", "red", BLINK_SLOW, FADE_MS);
}

void led_status_server_error(RgbLed* led) {
    // Vermelho, piscando rapidamente
    rgb_led_onled(led, "on", "red", BLINK_FAST, FADE_MS);
}
void led_status_ready(RgbLed* led) {
    // Verde, sólido
    rgb_led_onled(led, "on", "green", 0, 0);
}

void led_status_thinking(RgbLed* led) {
    // Azul, piscando lentamente
    rgb_led_onled(led, "on", "blue", BLINK_SLOW, FADE_MS);
}

void led_status_responding(RgbLed* led) {
    // Azul, piscando rápido
    rgb_led_onled(led, "on", "blue", BLINK_FAST, FADE_MS);
}

} // Fim do extern "C"