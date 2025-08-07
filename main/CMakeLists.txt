idf_component_register(SRCS "main.c" "mic.c" "speaker.c" "wifi.c" "audio.c" "webrtc.c" "http.c" "filters.c"
    INCLUDE_DIRS "." "includes"
    REQUIRES libpeer esp-libopus driver libpeer esp_wifi nvs_flash esp_http_client
)

idf_component_get_property(lib libpeer COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=restrict)
target_compile_options(${lib} PRIVATE -Wno-error=stringop-truncation)

idf_component_get_property(lib sepfy__srtp COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=incompatible-pointer-types)

idf_component_get_property(lib esp-libopus COMPONENT_LIB)
target_compile_options(${lib} PRIVATE -Wno-error=maybe-uninitialized)
target_compile_options(${lib} PRIVATE -Wno-error=stringop-overread)
