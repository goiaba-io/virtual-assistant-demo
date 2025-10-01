# Changelog

## [1.1.0] - 2025-09-25

This version introduces a visual feedback system using an RGB LED, significantly improving the interactivity and debuggability of the virtual assistant.

### Added
- **RGB LED Status Indicator:** Integrated support for a WS2812 LED (NeoPixel) to provide visual feedback on the virtual assistant's status.
- **`rgb_led` Control Module:** Created a new abstraction module (`rgb_led.h`, `rgb_led.cpp`) that acts as a C wrapper for the ESP-IDF `led_strip` component, facilitating LED control. - **New Visual States:** Multiple states have been defined for the LED, including:
- `Ready` (Solid Green): The assistant is idle and ready to receive commands.
- `Thinking` (Slowly blinking Yellow or Blue): The assistant is processing audio captured by the microphone.
- `Responding` (Fast blinking Blue): The assistant is speaking and playing audio.
- `Error` (Blinking Red): Indicates connection failures or critical errors.