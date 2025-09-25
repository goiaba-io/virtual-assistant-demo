#pragma once

#include <stdint.h>

#include "peer.h"

#ifdef __cplusplus
extern "C" {
#endif

void webrtc_init();
void webrtc_register_connection_task(void);
void webrtc_register_send_audio_task(void);
void init_led_activity_timer(void);

#ifdef __cplusplus
}
#endif
