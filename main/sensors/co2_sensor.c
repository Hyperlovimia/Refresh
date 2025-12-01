/**
 * @file co2_sensor.c
 * @brief CO2 传感器驱动（MH-Z19B UART 接口）
 */

#include "co2_sensor.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CO2_SENSOR";
static bool s_uart_ready = false;

#define CO2_SENSOR_UART_NUM UART_NUM_2
#define CO2_SENSOR_UART_TX  GPIO_NUM_17
#define CO2_SENSOR_UART_RX  GPIO_NUM_16
#define CO2_SENSOR_UART_BUF_SIZE 256

esp_err_t co2_sensor_init(void) {
    if (s_uart_ready) {
        return ESP_OK;
    }

    const uart_config_t uart_cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(CO2_SENSOR_UART_NUM, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed (%d)", err);
        return err;
    }

    err = uart_set_pin(
        CO2_SENSOR_UART_NUM,
        CO2_SENSOR_UART_TX,
        CO2_SENSOR_UART_RX,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed (%d)", err);
        return err;
    }

    err = uart_driver_install(CO2_SENSOR_UART_NUM, CO2_SENSOR_UART_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed (%d)", err);
        return err;
    }

    s_uart_ready = true;
    ESP_LOGI(TAG, "CO2 UART 初始化完成 GPIO16/17 9600 8N1");
    return ESP_OK;
}

float co2_sensor_read_ppm(void) {
    if (!s_uart_ready) {
        ESP_LOGE(TAG, "UART 未初始化");
        return -1.0f;
    }

    uint8_t buffer[32];
    int len = uart_read_bytes(CO2_SENSOR_UART_NUM, buffer, sizeof(buffer) - 1, pdMS_TO_TICKS(100));

    if (len <= 0) {
        // 无数据或超时
        return -1.0f;
    }

    buffer[len] = '\0';  // 确保字符串结尾

    // 查找 "ppm" 字符串（格式：  xxxx ppm\r\n）
    char *ppm_pos = strstr((char *)buffer, "ppm");
    if (!ppm_pos) {
        ESP_LOGW(TAG, "未找到 'ppm' 标志");
        return -1.0f;
    }

    // 从 ppm 位置向前查找数值起始位置
    char *value_start = ppm_pos - 1;
    while (value_start > (char *)buffer && *value_start == ' ') {
        value_start--;
    }
    while (value_start > (char *)buffer && (*value_start >= '0' && *value_start <= '9')) {
        value_start--;
    }
    if (*value_start < '0' || *value_start > '9') {
        value_start++;
    }

    // 提取数值
    char value_str[8];
    int i = 0;
    while (value_start < ppm_pos && (*value_start >= '0' && *value_start <= '9') && i < 7) {
        value_str[i++] = *value_start++;
    }
    value_str[i] = '\0';

    if (i == 0) {
        ESP_LOGW(TAG, "未找到有效数值");
        return -1.0f;
    }

    float co2_ppm = atof(value_str);

    // 范围验证（JX-CO2-102-5K: 0-5000 ppm）
    if (co2_ppm < 0.0f || co2_ppm > 5000.0f) {
        ESP_LOGW(TAG, "CO₂ 浓度超出范围: %.1f ppm", co2_ppm);
        return -1.0f;
    }

    return co2_ppm;
}

bool co2_sensor_is_ready(void) {
    // TODO: 实现 - 检查传感器是否准备好
    return true;
}

esp_err_t co2_sensor_calibrate(void) {
    ESP_LOGW(TAG, "校准功能暂不支持");
    return ESP_ERR_NOT_SUPPORTED;
}
