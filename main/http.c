#include "http.h"

#include <esp_http_client.h>
#include <esp_log.h>
#include <string.h>

#include "cJSON.h"
#include "utils.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

const char *TAG = "openai_http_client";

esp_err_t openai_http_event_handler(esp_http_client_event_t *evt) {
    static int output_len;
    switch (evt->event_id) {
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG,
                "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                evt->header_key,
                evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA: {
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGE(TAG, "Chunked HTTP response not supported");
                esp_restart();
            }

            if (output_len == 0 && evt->user_data) {
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            int copy_len = 0;
            if (evt->user_data) {
                copy_len =
                    MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len) {
                    memcpy(((char *)evt->user_data) + output_len,
                        evt->data,
                        copy_len);
                }
            }
            output_len += copy_len;

            break;
        }
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
    }
    return ESP_OK;
}

static esp_err_t http_evt_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data &&
        evt->data_len > 0) {
        struct bufctx {
            char *buf;
            size_t cap;
            size_t used;
        };
        struct bufctx *ctx = (struct bufctx *)evt->user_data;
        size_t room = (ctx->cap > ctx->used) ? (ctx->cap - ctx->used - 1) : 0;
        size_t to_copy = evt->data_len < room ? evt->data_len : room;
        if (to_copy > 0) {
            memcpy(ctx->buf + ctx->used, evt->data, to_copy);
            ctx->used += to_copy;
            ctx->buf[ctx->used] = '\0';
        }
    }
    return ESP_OK;
}

esp_err_t openai_create_realtime_session(const char *api_key, const char *model,
    const char *voice, char *token_out, size_t token_cap) {
    if (!api_key || !model || !voice || !token_out || token_cap < 8) {
        return ESP_ERR_INVALID_ARG;
    }
    token_out[0] = '\0';

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", model);
    cJSON_AddStringToObject(root, "voice", voice);

    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return ESP_ERR_NO_MEM;

    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", api_key);

    char resp_buf[2048];
    struct bufctx {
        char *buf;
        size_t cap;
        size_t used;
    } ctx = {.buf = resp_buf, .cap = sizeof(resp_buf), .used = 0};
    resp_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = "https://api.openai.com/v1/realtime/sessions",
        .method = HTTP_METHOD_POST,
        .event_handler = http_evt_handler,
        .user_data = &ctx,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) {
        free(body);
        return ESP_FAIL;
    }

    esp_http_client_set_header(cli, "Authorization", auth);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    esp_http_client_set_post_field(cli, body, strlen(body));

    esp_err_t err = esp_http_client_perform(cli);
    int status = esp_http_client_get_status_code(cli);

    free(body);
    esp_http_client_cleanup(cli);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP perform error: %s", esp_err_to_name(err));
        return err;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "HTTP status %d, body: %s", status, resp_buf);
        return ESP_FAIL;
    }

    cJSON *j = cJSON_Parse(resp_buf);
    if (!j) {
        ESP_LOGE(TAG, "JSON parse fail");
        return ESP_FAIL;
    }
    const cJSON *client_secret =
        cJSON_GetObjectItemCaseSensitive(j, "client_secret");
    const cJSON *value =
        client_secret ? cJSON_GetObjectItemCaseSensitive(client_secret, "value")
                      : NULL;

    if (cJSON_IsString(value) && value->valuestring && value->valuestring[0]) {
        strlcpy(token_out, value->valuestring, token_cap);
        cJSON_Delete(j);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "client_secret.value not found. Resp: %s", resp_buf);
        cJSON_Delete(j);
        return ESP_FAIL;
    }
}

void openai_http_request(char *offer, char *answer) {
    char eph[256];
    esp_err_t ok = openai_create_realtime_session(CONFIG_OPENAI_API_KEY,
        CONFIG_OPENAI_MODEL,
        CONFIG_OPENAI_VOICE,
        eph,
        sizeof(eph));
    if (ok == ESP_OK) {
        ESP_LOGI("OAI_SESSION", "Ephemeral token: %s", eph);
    } else {
        ESP_LOGE("OAI_SESSION", "Falha ao criar sessão");
        return;
    }

    esp_http_client_config_t config;
    memset(&config, 0, sizeof(esp_http_client_config_t));

    config.url = CONFIG_OPENAI_REALTIMEAPI;
    config.event_handler = openai_http_event_handler;
    config.user_data = answer;

    snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Bearer %s", eph);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/sdp");
    esp_http_client_set_header(client, "Authorization", answer);
    esp_http_client_set_post_field(client, offer, strlen(offer));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK || esp_http_client_get_status_code(client) != 201) {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        esp_restart();
    }

    esp_http_client_cleanup(client);
}
