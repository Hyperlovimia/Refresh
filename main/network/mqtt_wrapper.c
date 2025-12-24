/**
 * @file mqtt_wrapper.c
 * @brief MQTT 客户端包装实现 - EMQX Cloud TLS
 */

#include "mqtt_wrapper.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"    // ESP-IDF MQTT 库
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "MQTT_CLIENT";

// MQTT 主题定义
#define MQTT_TOPIC_STATUS   "home/ventilation/status"
#define MQTT_TOPIC_ALERT    "home/ventilation/alert"
#define MQTT_TOPIC_COMMAND  "home/ventilation/command/#"

// MQTT 重连配置
#define MQTT_RECONNECT_DELAY_MS  30000  // 30 秒重连间隔

// 全局变量
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static TimerHandle_t s_reconnect_timer = NULL;
static FanState s_remote_command[FAN_COUNT] = {FAN_OFF, FAN_OFF, FAN_OFF};
static bool s_command_received = false;

/**
 * @brief FanState 转字符串
 */
static const char* fan_state_to_string(FanState state)
{
    switch (state) {
        case FAN_OFF:
            return "OFF";
        case FAN_LOW:
            return "LOW";
        case FAN_HIGH:
            return "HIGH";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief SystemMode 转字符串
 */
static const char* system_mode_to_string(SystemMode mode)
{
    switch (mode) {
        case MODE_REMOTE:
            return "REMOTE";
        case MODE_LOCAL:
            return "LOCAL";
        case MODE_SAFE_STOP:
            return "SAFE_STOP";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 字符串转 FanState
 */
static FanState string_to_fan_state(const char *str)
{
    if (strcmp(str, "OFF") == 0) return FAN_OFF;
    if (strcmp(str, "LOW") == 0) return FAN_LOW;
    if (strcmp(str, "HIGH") == 0) return FAN_HIGH;
    return FAN_OFF;
}

/**
 * @brief 解析远程命令 JSON（支持多风扇）
 */
static void parse_remote_command(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) {
        ESP_LOGW(TAG, "远程命令 JSON 解析失败");
        return;
    }

    bool has_command = false;

    // 解析 fan_0, fan_1, fan_2
    for (int i = 0; i < FAN_COUNT; i++) {
        char key[16];
        snprintf(key, sizeof(key), "fan_%d", i);

        cJSON *fan_state = cJSON_GetObjectItem(root, key);
        if (cJSON_IsString(fan_state)) {
            s_remote_command[i] = string_to_fan_state(fan_state->valuestring);
            has_command = true;
            ESP_LOGI(TAG, "收到远程命令: %s=%s", key, fan_state->valuestring);
        }
    }

    if (has_command) {
        s_command_received = true;
    }

    cJSON_Delete(root);
}

/**
 * @brief 重连定时器回调（在定时器任务上下文中执行）
 */
static void reconnect_timer_callback(TimerHandle_t xTimer)
{
    if (s_mqtt_client != NULL && !s_mqtt_connected) {
        ESP_LOGI(TAG, "执行 MQTT 重连...");
        esp_mqtt_client_reconnect(s_mqtt_client);
    }
}

/**
 * @brief MQTT 事件处理器
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT 连接成功");
            s_mqtt_connected = true;

            // 停止重连定时器（如果正在运行）
            if (s_reconnect_timer != NULL && xTimerIsTimerActive(s_reconnect_timer)) {
                xTimerStop(s_reconnect_timer, 0);
            }

            // 订阅远程命令主题
            esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_COMMAND, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT 连接断开");
            s_mqtt_connected = false;

            // 启动重连定时器（30 秒后重连）
            if (s_reconnect_timer != NULL) {
                ESP_LOGI(TAG, "将在 %d 秒后重连 MQTT...", MQTT_RECONNECT_DELAY_MS / 1000);
                xTimerStart(s_reconnect_timer, 0);
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT 订阅成功，msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            // 处理接收到的消息
            if (event->topic_len > 0 && strncmp(event->topic, MQTT_TOPIC_COMMAND, event->topic_len) == 0) {
                parse_remote_command(event->data, event->data_len);
            }
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT 发布成功，msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT 错误事件");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "  - TCP 传输错误");
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "  - 连接被拒绝");
            }
            break;

        default:
            ESP_LOGD(TAG, "MQTT 其他事件: %d", event_id);
            break;
    }
}

esp_err_t mqtt_client_init(void)
{
    ESP_LOGI(TAG, "初始化 MQTT 客户端（EMQX Cloud TLS）");

    // 从 menuconfig 读取配置
    const char *broker_url = CONFIG_MQTT_BROKER_URL;
    const char *username = CONFIG_MQTT_USERNAME;
    const char *password = CONFIG_MQTT_PASSWORD;

    // 获取设备 MAC 地址作为 Client ID
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    static char client_id[32];  // static 确保指针在函数返回后有效
    snprintf(client_id, sizeof(client_id), "esp32_%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "MQTT Client ID: %s", client_id);

    // 配置遗嘱消息（设备异常掉线时发送）
    static const char *lwt_msg = "{\"online\": false}";

    // 配置 MQTT 客户端（简化配置，参考 ESP-IDF 示例）
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = broker_url,
            },
            .verification = {
                .crt_bundle_attach = esp_crt_bundle_attach,
            },
        },
        .credentials = {
            .username = username,
            .client_id = client_id,
            .authentication = {
                .password = password,
            },
        },
        .session = {
            .last_will = {
                .topic = MQTT_TOPIC_STATUS,
                .msg = lwt_msg,
                .msg_len = strlen(lwt_msg),
                .qos = 1,
                .retain = 0,  // int 类型
            },
        },
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT 客户端初始化失败");
        return ESP_FAIL;
    }

    // 注册事件处理器
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // 创建重连定时器（30 秒超时，单次触发）
    s_reconnect_timer = xTimerCreate("mqtt_reconnect",
                                     pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS),
                                     pdFALSE,  // 单次触发
                                     NULL,
                                     reconnect_timer_callback);
    if (s_reconnect_timer == NULL) {
        ESP_LOGE(TAG, "创建重连定时器失败");
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ESP_FAIL;
    }

    // 启动 MQTT 客户端
    esp_err_t err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MQTT 客户端启动失败: %s", esp_err_to_name(err));
        xTimerDelete(s_reconnect_timer, 0);
        s_reconnect_timer = NULL;
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MQTT 客户端启动成功");
    return ESP_OK;
}

esp_err_t mqtt_publish_status(SensorData *sensor, const FanState fans[FAN_COUNT], SystemMode mode)
{
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_mqtt_connected) {
        ESP_LOGW(TAG, "MQTT 未连接，跳过状态发布");
        return ESP_FAIL;
    }

    // 构建 JSON 消息
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "创建 JSON 对象失败");
        return ESP_FAIL;
    }

    cJSON_AddNumberToObject(root, "co2", sensor->pollutants.co2);
    cJSON_AddNumberToObject(root, "temp", sensor->temperature);
    cJSON_AddNumberToObject(root, "humi", sensor->humidity);

    // 添加3个风扇状态
    for (int i = 0; i < FAN_COUNT; i++) {
        char key[16];
        snprintf(key, sizeof(key), "fan_%d", i);
        cJSON_AddStringToObject(root, key, fan_state_to_string(fans[i]));
    }

    cJSON_AddStringToObject(root, "mode", system_mode_to_string(mode));

    struct timeval tv;
    gettimeofday(&tv, NULL);
    cJSON_AddNumberToObject(root, "timestamp", tv.tv_sec);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "序列化 JSON 失败");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 发布消息（QoS 0）
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_STATUS, json_str, 0, 0, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "MQTT 发布状态失败");
        cJSON_free(json_str);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "发布状态: %s (msg_id=%d)", json_str, msg_id);

    // 释放资源
    cJSON_free(json_str);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t mqtt_publish_alert(const char *message)
{
    if (!message) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_mqtt_connected) {
        ESP_LOGW(TAG, "MQTT 未连接，跳过告警发布");
        return ESP_FAIL;
    }

    // 构建 JSON 消息
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "创建 JSON 对象失败");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "alert", message);
    cJSON_AddStringToObject(root, "level", "WARNING");

    struct timeval tv;
    gettimeofday(&tv, NULL);
    cJSON_AddNumberToObject(root, "timestamp", tv.tv_sec);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "序列化 JSON 失败");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 发布消息（QoS 1，保证送达）
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_ALERT, json_str, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "MQTT 发布告警失败");
        cJSON_free(json_str);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "发布告警: %s (msg_id=%d, QoS=1)", json_str, msg_id);

    // 释放资源
    cJSON_free(json_str);
    cJSON_Delete(root);

    return ESP_OK;
}

bool mqtt_get_remote_command(FanState cmd[FAN_COUNT])
{
    if (!cmd || !s_mqtt_connected || !s_command_received) {
        return false;
    }
    for (int i = 0; i < FAN_COUNT; i++) {
        cmd[i] = s_remote_command[i];
    }
    return true;
}
