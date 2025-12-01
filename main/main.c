/**
 * @file main.c
 * @brief 智能室内空气质量改善系统 - 主程序
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "main.h"
#include "sensors/sensor_manager.h"
#include "actuators/fan_control.h"
#include "algorithm/decision_engine.h"
#include "network/wifi_manager.h"
#include "network/weather_api.h"
#include "network/mqtt_client.h"
#include "ui/oled_display.h"

// ============================================================================
// 日志标签
// ============================================================================
static const char *TAG = "MAIN";

// ============================================================================
// 全局变量和同步对象
// ============================================================================

// 系统状态
static SystemState current_state = STATE_INIT;
static SystemMode current_mode = MODE_LOCAL;

// 共享数据缓冲区
static SensorData shared_sensor_data = {0};
static WeatherData shared_weather_data = {0};
static FanState shared_fan_state = FAN_OFF;

// 同步对象
static SemaphoreHandle_t data_mutex = NULL;
static EventGroupHandle_t system_events = NULL;
static QueueHandle_t alert_queue = NULL;

// 事件组位定义
#define EVENT_WIFI_CONNECTED    BIT0
#define EVENT_SENSOR_READY      BIT1
#define EVENT_SENSOR_STABLE     BIT2
#define EVENT_SENSOR_FAULT      BIT3

// ============================================================================
// 系统状态转换函数
// ============================================================================

/**
 * @brief 状态转换处理
 */
static void state_transition(SystemState new_state) {
    if (current_state == new_state) {
        return;
    }

    ESP_LOGI(TAG, "状态转换: %d → %d", current_state, new_state);
    current_state = new_state;
}

/**
 * @brief 处理初始化状态
 */
static void handle_init_state(void) {
    ESP_LOGI(TAG, "进入 INIT 状态");
    // 状态机将在所有模块初始化成功后自动转换到 PREHEATING
}

/**
 * @brief 处理预热状态
 */
static void handle_preheating_state(void) {
    static uint32_t preheating_start = 0;

    if (preheating_start == 0) {
        preheating_start = xTaskGetTickCount();
        ESP_LOGI(TAG, "进入 PREHEATING 状态（60秒倒计时）");
        oled_display_alert("传感器预热中...");
    }

    // 检查是否超时
    uint32_t elapsed = (xTaskGetTickCount() - preheating_start) / configTICK_RATE_HZ;
    if (elapsed >= PREHEATING_TIME_SEC) {
        ESP_LOGI(TAG, "预热完成");
        xEventGroupSetBits(system_events, EVENT_SENSOR_READY);
        preheating_start = 0;
        state_transition(STATE_STABILIZING);
    }
}

/**
 * @brief 处理稳定状态
 */
static void handle_stabilizing_state(void) {
    static uint32_t stabilizing_start = 0;

    if (stabilizing_start == 0) {
        stabilizing_start = xTaskGetTickCount();
        ESP_LOGI(TAG, "进入 STABILIZING 状态（240秒倒计时）");
        oled_display_alert("传感器稳定中...");
    }

    // 检查是否超时
    uint32_t elapsed = (xTaskGetTickCount() - stabilizing_start) / configTICK_RATE_HZ;
    if (elapsed >= STABILIZING_TIME_SEC) {
        ESP_LOGI(TAG, "稳定完成");
        xEventGroupSetBits(system_events, EVENT_SENSOR_STABLE);
        stabilizing_start = 0;
        state_transition(STATE_RUNNING);
    }
}

/**
 * @brief 处理运行状态
 */
static void handle_running_state(void) {
    // 正常运行，监控传感器健康状态
    if (!sensor_manager_is_healthy()) {
        ESP_LOGE(TAG, "传感器故障检测");
        xEventGroupSetBits(system_events, EVENT_SENSOR_FAULT);
        state_transition(STATE_ERROR);
    }
}

/**
 * @brief 处理错误状态
 */
static void handle_error_state(void) {
    static uint32_t error_start = 0;

    if (error_start == 0) {
        error_start = xTaskGetTickCount();
        ESP_LOGE(TAG, "进入 ERROR 状态（安全停机）");

        // 关闭风扇
        fan_control_set_state(FAN_OFF, false);

        // 显示告警
        oled_display_alert("传感器故障");
        mqtt_publish_alert("Sensor fault detected");
    }

    // 每10秒检查一次传感器恢复状态
    uint32_t elapsed = (xTaskGetTickCount() - error_start) / configTICK_RATE_HZ;
    if (elapsed >= 10) {
        error_start = 0;

        if (sensor_manager_is_healthy()) {
            ESP_LOGI(TAG, "传感器恢复，重新初始化系统");
            xEventGroupClearBits(system_events, EVENT_SENSOR_FAULT);
            state_transition(STATE_INIT);
        }
    }
}

// ============================================================================
// FreeRTOS 任务
// ============================================================================

/**
 * @brief 传感器任务（1Hz）
 */
static void sensor_task(void *pvParameters) {
    SensorData data;

    ESP_LOGI(TAG, "传感器任务启动");

    while (1) {
        // 读取传感器数据
        esp_err_t ret = sensor_manager_read_all(&data);

        // 写入共享缓冲区
        xSemaphoreTake(data_mutex, portMAX_DELAY);
        if (ret == ESP_OK && data.valid) {
            memcpy(&shared_sensor_data, &data, sizeof(SensorData));
        }
        xSemaphoreGive(data_mutex);

        // 检查CO₂告警
        if (data.valid && data.co2 > CO2_ALERT_THRESHOLD) {
            char alert_msg[64];
            snprintf(alert_msg, sizeof(alert_msg), "CO₂浓度过高: %.0f ppm", data.co2);
            xQueueSend(alert_queue, &alert_msg, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz
    }
}

/**
 * @brief 决策任务（1Hz）
 */
static void decision_task(void *pvParameters) {
    SensorData sensor;
    WeatherData weather;
    FanState new_state;

    ESP_LOGI(TAG, "决策任务启动");

    while (1) {
        // 等待稳定状态完成
        if (current_state < STATE_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // 读取共享数据
        xSemaphoreTake(data_mutex, portMAX_DELAY);
        memcpy(&sensor, &shared_sensor_data, sizeof(SensorData));
        memcpy(&weather, &shared_weather_data, sizeof(WeatherData));
        FanState old_state = shared_fan_state;
        xSemaphoreGive(data_mutex);

        // 检测运行模式
        bool wifi_ok = wifi_manager_is_connected();
        bool cache_valid = !weather_api_is_cache_stale();
        bool sensor_ok = sensor_manager_is_healthy();
        current_mode = decision_detect_mode(wifi_ok, cache_valid, sensor_ok);

        // 执行决策
        new_state = decision_make(&sensor, &weather, current_mode);

        // 设置风扇状态（根据时间判断昼夜模式）
        // TODO: 实现昼夜判断逻辑（22:00-8:00为夜间）
        bool is_night = false; // 简化实现，默认白天模式

        if (new_state != old_state) {
            fan_control_set_state(new_state, is_night);

            xSemaphoreTake(data_mutex, portMAX_DELAY);
            shared_fan_state = new_state;
            xSemaphoreGive(data_mutex);

            ESP_LOGI(TAG, "风扇状态变化: %d → %d", old_state, new_state);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz
    }
}

/**
 * @brief 网络任务（10分钟周期）
 */
static void network_task(void *pvParameters) {
    WeatherData weather;
    SensorData sensor;
    FanState fan;

    ESP_LOGI(TAG, "网络任务启动");

    while (1) {
        // 检查 WiFi 连接
        if (wifi_manager_is_connected()) {
            xEventGroupSetBits(system_events, EVENT_WIFI_CONNECTED);

            // 获取天气数据
            if (weather_api_fetch(&weather) == ESP_OK) {
                xSemaphoreTake(data_mutex, portMAX_DELAY);
                memcpy(&shared_weather_data, &weather, sizeof(WeatherData));
                xSemaphoreGive(data_mutex);
            }

            // 上报 MQTT 状态
            xSemaphoreTake(data_mutex, portMAX_DELAY);
            memcpy(&sensor, &shared_sensor_data, sizeof(SensorData));
            fan = shared_fan_state;
            xSemaphoreGive(data_mutex);

            mqtt_publish_status(&sensor, fan);
        } else {
            xEventGroupClearBits(system_events, EVENT_WIFI_CONNECTED);
        }

        vTaskDelay(pdMS_TO_TICKS(WEATHER_FETCH_INTERVAL_SEC * 1000)); // 10分钟
    }
}

/**
 * @brief 显示任务（0.5Hz）
 */
static void display_task(void *pvParameters) {
    SensorData sensor;
    WeatherData weather;
    FanState fan;
    char alert_msg[64];

    ESP_LOGI(TAG, "显示任务启动");

    while (1) {
        // 检查告警队列
        if (xQueueReceive(alert_queue, &alert_msg, 0) == pdTRUE) {
            oled_display_alert(alert_msg);
            vTaskDelay(pdMS_TO_TICKS(2000)); // 显示2秒告警
        }

        // 更新主页面
        if (current_state == STATE_RUNNING) {
            xSemaphoreTake(data_mutex, portMAX_DELAY);
            memcpy(&sensor, &shared_sensor_data, sizeof(SensorData));
            memcpy(&weather, &shared_weather_data, sizeof(WeatherData));
            fan = shared_fan_state;
            xSemaphoreGive(data_mutex);

            oled_display_main_page(&sensor, &weather, fan, current_mode);
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // 0.5Hz
    }
}

// ============================================================================
// 系统初始化函数
// ============================================================================

/**
 * @brief 系统初始化
 */
static esp_err_t system_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "系统启动：智能室内空气质量改善系统");
    ESP_LOGI(TAG, "============================================");

    // 初始化 NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS分区已满或版本不匹配，擦除后重新初始化");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS初始化失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ NVS初始化成功");

    // 创建同步对象
    data_mutex = xSemaphoreCreateMutex();
    system_events = xEventGroupCreate();
    alert_queue = xQueueCreate(5, sizeof(char) * 64);

    if (!data_mutex || !system_events || !alert_queue) {
        ESP_LOGE(TAG, "同步对象创建失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ 同步对象创建成功");

    // 初始化传感器管理器
    ret = sensor_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "✗ 传感器管理器初始化失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ 传感器管理器初始化成功");

    // 初始化风扇控制
    ret = fan_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "✗ 风扇控制初始化失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ 风扇控制初始化成功");

    // 初始化 WiFi 管理器
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠ WiFi管理器初始化失败（继续运行）");
    } else {
        ESP_LOGI(TAG, "✓ WiFi管理器初始化成功");
    }

    // 初始化天气 API
    ret = weather_api_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠ 天气API初始化失败（继续运行）");
    } else {
        ESP_LOGI(TAG, "✓ 天气API初始化成功");
    }

    // 初始化 MQTT 客户端
    ret = mqtt_client_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠ MQTT客户端初始化失败（继续运行）");
    } else {
        ESP_LOGI(TAG, "✓ MQTT客户端初始化成功");
    }

    // 初始化 OLED 显示
    ret = oled_display_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠ OLED显示初始化失败（继续运行）");
    } else {
        ESP_LOGI(TAG, "✓ OLED显示初始化成功");
    }

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "系统初始化完成");
    ESP_LOGI(TAG, "============================================");

    return ESP_OK;
}

// ============================================================================
// 主函数
// ============================================================================

void app_main(void) {
    // 系统初始化
    if (system_init() != ESP_OK) {
        ESP_LOGE(TAG, "系统初始化失败，进入错误状态");
        state_transition(STATE_ERROR);
        while (1) {
            handle_error_state();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // 进入预热状态
    state_transition(STATE_PREHEATING);

    // 创建任务
    xTaskCreate(sensor_task, "sensor", TASK_STACK_SIZE_SMALL, NULL, TASK_PRIORITY_SENSOR, NULL);
    xTaskCreate(decision_task, "decision", TASK_STACK_SIZE_SMALL, NULL, TASK_PRIORITY_DECISION, NULL);
    xTaskCreate(network_task, "network", TASK_STACK_SIZE_LARGE, NULL, TASK_PRIORITY_NETWORK, NULL);
    xTaskCreate(display_task, "display", TASK_STACK_SIZE_SMALL, NULL, TASK_PRIORITY_DISPLAY, NULL);

    ESP_LOGI(TAG, "所有任务已创建");

    // 主循环：状态机管理
    while (1) {
        switch (current_state) {
            case STATE_INIT:
                handle_init_state();
                break;

            case STATE_PREHEATING:
                handle_preheating_state();
                break;

            case STATE_STABILIZING:
                handle_stabilizing_state();
                break;

            case STATE_RUNNING:
                handle_running_state();
                break;

            case STATE_ERROR:
                handle_error_state();
                break;

            default:
                ESP_LOGE(TAG, "未知状态：%d", current_state);
                state_transition(STATE_ERROR);
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz状态机循环
    }
}
