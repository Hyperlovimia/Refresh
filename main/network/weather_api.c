/**
 * @file weather_api.c
 * @brief 天气 API 客户端 - 和风天气 HTTPS 实现
 */

#include "weather_api.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "WEATHER_API";

// API 配置（通过 menuconfig 配置）
#define API_URL_BASE        "https://api.qweather.com/v7/air/now"
#define HTTP_RESPONSE_BUFFER_SIZE   2048
#define HTTP_TIMEOUT_MS     10000  // 10 秒超时

// 缓存数据
static WeatherData cached_data = {0};

// HTTP 响应缓冲区
static char http_response_buffer[HTTP_RESPONSE_BUFFER_SIZE];
static int http_response_len = 0;

/**
 * @brief HTTP 事件处理器
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // 接收数据
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (http_response_len + evt->data_len < HTTP_RESPONSE_BUFFER_SIZE) {
                    memcpy(http_response_buffer + http_response_len, evt->data, evt->data_len);
                    http_response_len += evt->data_len;
                } else {
                    ESP_LOGW(TAG, "HTTP 响应缓冲区不足，截断数据");
                }
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief 解析 JSON 响应，提取天气数据
 * @param json_str JSON 字符串
 * @param data 输出天气数据
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
static esp_err_t parse_weather_json(const char *json_str, WeatherData *data)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON 解析失败");
        return ESP_FAIL;
    }

    // 检查响应码
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code == NULL || !cJSON_IsString(code)) {
        ESP_LOGE(TAG, "JSON 响应缺少 'code' 字段");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (strcmp(code->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "API 返回错误码: %s", code->valuestring);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 解析 "now" 对象
    cJSON *now = cJSON_GetObjectItem(root, "now");
    if (now == NULL || !cJSON_IsObject(now)) {
        ESP_LOGE(TAG, "JSON 响应缺少 'now' 对象");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 提取 PM2.5
    cJSON *pm2p5 = cJSON_GetObjectItem(now, "pm2p5");
    if (pm2p5 == NULL || !cJSON_IsString(pm2p5)) {
        ESP_LOGE(TAG, "JSON 响应缺少 'pm2p5' 字段");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    data->pm25 = atof(pm2p5->valuestring);

    // 提取温度（注意：和风天气 API 的 /air/now 接口不返回温度和风速）
    // 需要从 /weather/now 接口获取，这里我们使用固定值或从其他接口获取
    // 为了简化，我们暂时设置为默认值，实际使用时需要调用 /weather/now 接口
    ESP_LOGW(TAG, "注意：/air/now 接口不返回温度和风速，使用默认值");
    data->temperature = 20.0f;  // 默认值
    data->wind_speed = 0.0f;    // 默认值

    // 设置时间戳和有效标志
    struct timeval tv;
    gettimeofday(&tv, NULL);
    data->timestamp = tv.tv_sec;
    data->valid = true;

    cJSON_Delete(root);
    ESP_LOGI(TAG, "成功解析天气数据：PM2.5=%.1f μg/m³", data->pm25);
    return ESP_OK;
}

esp_err_t weather_api_init(void)
{
    ESP_LOGI(TAG, "初始化天气 API 客户端");

    // 清空缓存
    memset(&cached_data, 0, sizeof(WeatherData));
    cached_data.valid = false;

    // 清空响应缓冲区
    http_response_len = 0;
    memset(http_response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);

    ESP_LOGI(TAG, "天气 API 客户端初始化完成");
    return ESP_OK;
}

esp_err_t weather_api_fetch(WeatherData *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "获取天气数据（和风天气 API）");

    // 从 menuconfig 读取配置（编译时配置）
    // 注意：这里使用占位符，实际需要通过 Kconfig 配置
    const char *api_key = CONFIG_WEATHER_API_KEY;
    const char *location = CONFIG_WEATHER_CITY_CODE;

    // 构建完整 URL
    char url[256];
    snprintf(url, sizeof(url), "%s?location=%s&key=%s", API_URL_BASE, location, api_key);

    // 清空响应缓冲区
    http_response_len = 0;
    memset(http_response_buffer, 0, HTTP_RESPONSE_BUFFER_SIZE);

    // 配置 HTTP 客户端
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,  // 使用 ESP-IDF 内置根证书
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP 客户端初始化失败");
        return ESP_FAIL;
    }

    // 执行 HTTP GET 请求
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 请求失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // 检查 HTTP 状态码
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP 状态码错误: %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HTTP 请求成功，状态码: %d，响应长度: %d", status_code, http_response_len);

    // 清理 HTTP 客户端
    esp_http_client_cleanup(client);

    // 解析 JSON 响应
    http_response_buffer[http_response_len] = '\0';  // 确保字符串结尾
    ESP_LOGD(TAG, "HTTP 响应: %s", http_response_buffer);

    err = parse_weather_json(http_response_buffer, data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "解析 JSON 失败");
        return ESP_FAIL;
    }

    // 缓存数据
    memcpy(&cached_data, data, sizeof(WeatherData));

    return ESP_OK;
}

void weather_api_get_cached(WeatherData *data)
{
    if (data) {
        memcpy(data, &cached_data, sizeof(WeatherData));
    }
}

bool weather_api_is_cache_stale(void)
{
    if (!cached_data.valid) {
        return true;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t elapsed = tv.tv_sec - cached_data.timestamp;

    return (elapsed > WEATHER_CACHE_VALID_SEC);
}
