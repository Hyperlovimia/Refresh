/**
 * @file oled_display.c
 * @brief OLED 显示驱动模块
 */

#include "oled_display.h"
#include "u8g2_esp32_hal.h"
#include "../main.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "OLED_DISPLAY";

// ============================================================================
// 常量定义
// ============================================================================

#define HISTORY_BUFFER_SIZE 60  // 10小时，每10分钟一个点
#define I2C_PORT I2C_NUM_0
#define SDA_PIN 21
#define SCL_PIN 22
#define OLED_I2C_ADDRESS 0x3C

// ============================================================================
// 数据结构
// ============================================================================

/**
 * @brief 历史数据环形缓冲区
 */
typedef struct {
    float co2_values[HISTORY_BUFFER_SIZE];
    uint8_t head;       // 写入位置
    uint8_t count;      // 有效数据数量
} HistoryBuffer;

// ============================================================================
// 全局变量
// ============================================================================

static u8g2_t g_u8g2;
static HistoryBuffer g_history = {0};
static bool g_initialized = false;
static bool g_alert_active = false;
static bool g_alert_blink_state = false;
static TimerHandle_t g_alert_timer = NULL;
static TimerHandle_t g_blink_timer = NULL;
static char g_alert_message[64] = {0};  // 缓存告警消息
static uint8_t g_alert_countdown = 3;   // 倒计时秒数

// ============================================================================
// 内部函数声明
// ============================================================================

static void draw_co2_value(SensorData *sensor);
static void draw_mode_badge(SystemMode mode);
static void draw_temp_humidity(SensorData *sensor);
static void draw_trend_graph(void);
static void draw_status_bar(FanState fan, SystemMode mode);
static void alert_timer_callback(TimerHandle_t timer);
static void blink_timer_callback(TimerHandle_t timer);
static void draw_alert_page(void);

// ============================================================================
// 公共函数实现
// ============================================================================

esp_err_t oled_display_init(void) {
    ESP_LOGI(TAG, "初始化 OLED 显示器");

    // 配置 HAL
    u8g2_esp32_hal_t hal = {
        .i2c_port = I2C_PORT,
        .sda_pin = SDA_PIN,
        .scl_pin = SCL_PIN,
        .clk_speed = 400000  // 400kHz
    };

    // 初始化 u8g2（全缓冲模式）
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &g_u8g2,
        U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );

    // 设置 I2C 地址（8-bit 格式，已左移）
    u8g2_SetI2CAddress(&g_u8g2, OLED_I2C_ADDRESS << 1);  // 0x3C << 1 = 0x78

    // 初始化 HAL
    u8g2_esp32_hal_init(hal, &g_u8g2);

    // 初始化显示
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0);  // 唤醒显示
    u8g2_SetFontMode(&g_u8g2, 1);   // 启用透明字体模式（支持 UTF-8）
    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SendBuffer(&g_u8g2);

    // 创建定时器
    g_alert_timer = xTimerCreate("alert_timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, alert_timer_callback);  // 1秒周期，自动重载
    g_blink_timer = xTimerCreate("blink_timer", pdMS_TO_TICKS(500), pdTRUE, NULL, blink_timer_callback);   // 0.5秒周期，1Hz闪烁

    if (!g_alert_timer || !g_blink_timer) {
        ESP_LOGE(TAG, "创建定时器失败");
        return ESP_FAIL;
    }

    g_initialized = true;
    ESP_LOGI(TAG, "OLED 初始化完成");
    return ESP_OK;
}

void oled_display_main_page(SensorData *sensor, FanState fan, SystemMode mode) {
    if (!g_initialized || !sensor) {
        return;
    }

    // 如果告警激活，跳过主页面刷新
    if (g_alert_active) {
        return;
    }

    // 获取 I2C 锁保护显示操作
    SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        u8g2_ClearBuffer(&g_u8g2);

        // 绘制各个组件
        draw_co2_value(sensor);
        draw_mode_badge(mode);
        draw_temp_humidity(sensor);
        draw_trend_graph();
        draw_status_bar(fan, mode);

        u8g2_SendBuffer(&g_u8g2);
        xSemaphoreGive(i2c_mutex);

        ESP_LOGD(TAG, "刷新主页面");
    } else {
        ESP_LOGW(TAG, "获取 I2C 锁超时，跳过本次刷新");
    }
}

void oled_display_alert(const char *message) {
    if (!g_initialized || !message) {
        return;
    }

    ESP_LOGW(TAG, "显示告警: %s", message);

    // 缓存告警消息
    strncpy(g_alert_message, message, sizeof(g_alert_message) - 1);
    g_alert_message[sizeof(g_alert_message) - 1] = '\0';

    // 激活告警模式
    g_alert_active = true;
    g_alert_blink_state = true;
    g_alert_countdown = 3;

    // 启动定时器
    xTimerStart(g_alert_timer, 0);
    xTimerStart(g_blink_timer, 0);

    // 立即绘制第一帧
    draw_alert_page();
}

void oled_add_history_point(SensorData *sensor) {
    if (!sensor || !sensor->valid) {
        return;
    }

    float co2 = sensor->pollutants.co2;

    // 添加到环形缓冲区
    g_history.co2_values[g_history.head] = co2;
    g_history.head = (g_history.head + 1) % HISTORY_BUFFER_SIZE;

    if (g_history.count < HISTORY_BUFFER_SIZE) {
        g_history.count++;
    }

    ESP_LOGD(TAG, "添加历史点: %.1f ppm (count=%d)", co2, g_history.count);
}

// ============================================================================
// 内部函数实现
// ============================================================================

static void draw_co2_value(SensorData *sensor) {
    char buf[32];

    // 大字号显示 CO2 数值
    u8g2_SetFont(&g_u8g2, u8g2_font_logisoso16_tn);

    if (sensor->valid) {
        snprintf(buf, sizeof(buf), "%.0f", sensor->pollutants.co2);
        u8g2_DrawStr(&g_u8g2, 0, 15, buf);

        // 状态标签
        u8g2_SetFont(&g_u8g2, u8g2_font_unifont_t_chinese2);
        const char *status = "OK";
        if (sensor->pollutants.co2 > CO2_ALERT_THRESHOLD) {
            status = "HIGH!";
        } else if (sensor->pollutants.co2 > CO2_THRESHOLD_HIGH) {
            status = "High";
        } else if (sensor->pollutants.co2 > CO2_THRESHOLD_LOW) {
            status = "Mid";
        }
        u8g2_DrawStr(&g_u8g2, 60, 16, status);
    } else {
        u8g2_DrawStr(&g_u8g2, 0, 15, "---");
    }
}

static void draw_mode_badge(SystemMode mode) {
    u8g2_SetFont(&g_u8g2, u8g2_font_unifont_t_chinese2);

    const char *badge = "";
    switch (mode) {
        case MODE_REMOTE:
            badge = "[RMT]";
            break;
        case MODE_LOCAL:
            badge = "[LOC]";
            break;
        case MODE_SAFE_STOP:
            badge = "[STOP]";
            break;
    }

    u8g2_DrawStr(&g_u8g2, 95, 16, badge);
}

static void draw_temp_humidity(SensorData *sensor) {
    char buf[32];

    u8g2_SetFont(&g_u8g2, u8g2_font_unifont_t_chinese2);

    if (sensor->valid) {
        snprintf(buf, sizeof(buf), "T:%.1fC H:%.0f%%", sensor->temperature, sensor->humidity);
    } else {
        snprintf(buf, sizeof(buf), "T:-- H:--");
    }

    u8g2_DrawStr(&g_u8g2, 0, 25, buf);
}

static void draw_trend_graph(void) {
    if (g_history.count < 2) {
        return;  // 至少需要 2 个点才能绘图
    }

    // 趋势图区域：y=26-55 (30px高度)
    const int graph_x = 0;
    const int graph_y = 26;
    const int graph_w = 128;
    const int graph_h = 30;

    // Y 轴范围：400-2000 ppm
    const float y_min = 400.0f;
    const float y_max = 2000.0f;

    // 绘制边框
    u8g2_DrawFrame(&g_u8g2, graph_x, graph_y, graph_w, graph_h);

    // 绘制基准线 (1000ppm)
    int baseline_y = graph_y + graph_h - (int)((1000.0f - y_min) / (y_max - y_min) * graph_h);
    for (int x = graph_x; x < graph_x + graph_w; x += 4) {
        u8g2_DrawPixel(&g_u8g2, x, baseline_y);
    }

    // 绘制告警线 (1500ppm, 虚线)
    int alert_y = graph_y + graph_h - (int)((1500.0f - y_min) / (y_max - y_min) * graph_h);
    for (int x = graph_x; x < graph_x + graph_w; x += 2) {
        if ((x / 2) % 2 == 0) {
            u8g2_DrawPixel(&g_u8g2, x, alert_y);
        }
    }

    // 绘制数据点
    int points_to_draw = g_history.count < graph_w ? g_history.count : graph_w;
    int start_idx = (g_history.head - points_to_draw + HISTORY_BUFFER_SIZE) % HISTORY_BUFFER_SIZE;

    for (int i = 0; i < points_to_draw - 1; i++) {
        int idx1 = (start_idx + i) % HISTORY_BUFFER_SIZE;
        int idx2 = (start_idx + i + 1) % HISTORY_BUFFER_SIZE;

        float co2_1 = g_history.co2_values[idx1];
        float co2_2 = g_history.co2_values[idx2];

        // 限制范围
        if (co2_1 < y_min) co2_1 = y_min;
        if (co2_1 > y_max) co2_1 = y_max;
        if (co2_2 < y_min) co2_2 = y_min;
        if (co2_2 > y_max) co2_2 = y_max;

        int x1 = graph_x + (i * graph_w / points_to_draw);
        int y1 = graph_y + graph_h - (int)((co2_1 - y_min) / (y_max - y_min) * graph_h);
        int x2 = graph_x + ((i + 1) * graph_w / points_to_draw);
        int y2 = graph_y + graph_h - (int)((co2_2 - y_min) / (y_max - y_min) * graph_h);

        u8g2_DrawLine(&g_u8g2, x1, y1, x2, y2);
    }
}

static void draw_status_bar(FanState fan, SystemMode mode) {
    char buf[32];

    u8g2_SetFont(&g_u8g2, u8g2_font_unifont_t_chinese2);

    // 风扇状态
    const char *fan_str = "OFF";
    switch (fan) {
        case FAN_LOW:
            fan_str = "LOW";
            break;
        case FAN_HIGH:
            fan_str = "HIGH";
            break;
        default:
            break;
    }

    snprintf(buf, sizeof(buf), "Fan:%s", fan_str);
    u8g2_DrawStr(&g_u8g2, 0, 63, buf);

    // WiFi 状态（简化显示）
    const char *wifi_str = (mode == MODE_LOCAL) ? "WiFi:OFF" : "WiFi:ON";
    u8g2_DrawStr(&g_u8g2, 70, 63, wifi_str);
}

static void draw_alert_page(void) {
    // 获取 I2C 锁保护显示操作
    SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        u8g2_ClearBuffer(&g_u8g2);

        // 绘制告警页面
        u8g2_SetFont(&g_u8g2, u8g2_font_10x20_tf);

        // 闪烁图标
        if (g_alert_blink_state) {
            u8g2_DrawStr(&g_u8g2, 30, 20, "! ! !");
        }

        // 告警消息
        u8g2_SetFont(&g_u8g2, u8g2_font_unifont_t_chinese2);
        u8g2_DrawStr(&g_u8g2, 20, 40, g_alert_message);

        // 倒计时提示
        char countdown_str[32];
        snprintf(countdown_str, sizeof(countdown_str), "%ds auto return", g_alert_countdown);
        u8g2_DrawStr(&g_u8g2, 10, 60, countdown_str);

        u8g2_SendBuffer(&g_u8g2);
        xSemaphoreGive(i2c_mutex);
    }
}

static void alert_timer_callback(TimerHandle_t timer) {
    // 每秒递减倒计时
    if (g_alert_countdown > 0) {
        g_alert_countdown--;
        draw_alert_page();  // 更新倒计时显示
    }

    if (g_alert_countdown == 0) {
        // 3 秒后关闭告警
        g_alert_active = false;
        xTimerStop(g_blink_timer, 0);
        xTimerStop(g_alert_timer, 0);
        ESP_LOGI(TAG, "告警自动关闭");
    }
}

static void blink_timer_callback(TimerHandle_t timer) {
    // 切换闪烁状态并重绘
    g_alert_blink_state = !g_alert_blink_state;
    if (g_alert_active) {
        draw_alert_page();
    }
}
