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
static uint32_t s_init_time = 0;  // 初始化时间（秒）

#define CO2_SENSOR_UART_NUM UART_NUM_2
#define CO2_SENSOR_UART_TX  GPIO_NUM_17
#define CO2_SENSOR_UART_RX  GPIO_NUM_16
#define CO2_SENSOR_UART_BUF_SIZE 1024  // 规格要求 1024 字节以容纳多帧

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
    s_init_time = xTaskGetTickCount() / configTICK_RATE_HZ;  // 记录初始化时间
    ESP_LOGI(TAG, "CO2 UART 初始化完成 GPIO16/17 9600 8N1");
    return ESP_OK;
}

float co2_sensor_read_ppm(void) {
    if (!s_uart_ready) {
        ESP_LOGE(TAG, "UART 未初始化");
        return -1.0f;
    }

    uint8_t buffer[64];
    int total_len = 0;
    int attempts = 0;

    // 尝试多次读取，拼接到 buffer，直到遇到换行或达到缓冲区
    while (attempts < 3 && total_len < (int)sizeof(buffer) - 1) {
        int len = uart_read_bytes(CO2_SENSOR_UART_NUM, buffer + total_len, sizeof(buffer) - 1 - total_len, pdMS_TO_TICKS(300));
        if (len > 0) {
            total_len += len;
            // 如果读到换行，认为一帧结束
            if (memchr(buffer, '\n', total_len)) {
                break;
            }
        } else {
            // 无数据，本次尝试结束
            break;
        }
        attempts++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (total_len <= 0) {
        // 无数据或超时
        ESP_LOGW(TAG, "CO2 raw read timeout");
        return -1.0f;
    }

    buffer[total_len] = '\0';  // 确保字符串结尾
    ESP_LOGI(TAG, "CO2 raw (%d bytes): '%s'", total_len, (char *)buffer);

    // 查找 "ppm" 字符串（格式：  xxxx ppm\r\n）
    char *ppm_pos = strstr((char *)buffer, "ppm");
    if (!ppm_pos) {
        ESP_LOGW(TAG, "未找到 'ppm' 标志 (raw='%s')", (char *)buffer);
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
        ESP_LOGW(TAG, "未找到有效数值 (raw='%s')", (char *)buffer);
        return -1.0f;
    }

    float co2_ppm = atof(value_str);
    ESP_LOGI(TAG, "CO2 parsed value_str='%s' -> %.1f ppm", value_str, co2_ppm);

    // 范围验证（JX-CO2-102-5K: 0-5000 ppm）
    if (co2_ppm < 0.0f || co2_ppm > 5000.0f) {
        ESP_LOGW(TAG, "CO₂ 浓度超出范围: %.1f ppm", co2_ppm);
        return -1.0f;
    }

    return co2_ppm;
}

bool co2_sensor_is_ready(void) {
    if (!s_uart_ready) {
        return false;
    }

    // 计算从初始化到现在的时间（秒）
    uint32_t elapsed = (xTaskGetTickCount() / configTICK_RATE_HZ) - s_init_time;

    // 规格要求：预热 60 秒后可用，300 秒后完全稳定
    // 这里使用 300 秒作为"就绪"标准
    if (elapsed < 300) {
        ESP_LOGD(TAG, "CO₂ 传感器预热中，已运行 %lu 秒（需要 300 秒）", elapsed);
        return false;
    }

    return true;
}

esp_err_t co2_sensor_calibrate(void) {
    if (!s_uart_ready) {
        ESP_LOGE(TAG, "UART 未初始化");
        return ESP_FAIL;
    }

    // 根据 JX-CO2-102 手册，手动校准命令
    // 发送：FF 01 05 07 00 00 00 00 F4
    // 接收：FF 01 03 07 01 00 00 00 F5（校准成功）
    uint8_t cmd[9] = {0xFF, 0x01, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0xF4};

    // 发送校准命令
    int len = uart_write_bytes(CO2_SENSOR_UART_NUM, cmd, sizeof(cmd));
    if (len != sizeof(cmd)) {
        ESP_LOGE(TAG, "发送校准命令失败");
        return ESP_FAIL;
    }

    // 等待响应（超时 2 秒）
    uint8_t resp[9];
    len = uart_read_bytes(CO2_SENSOR_UART_NUM, resp, sizeof(resp), pdMS_TO_TICKS(2000));
    if (len != sizeof(resp)) {
        ESP_LOGW(TAG, "未收到校准响应");
        return ESP_ERR_TIMEOUT;
    }

    // 验证响应：FF 01 03 07 01 00 00 00 F5
    if (resp[0] == 0xFF && resp[1] == 0x01 && resp[2] == 0x03 && resp[3] == 0x07 && resp[4] == 0x01) {
        ESP_LOGI(TAG, "CO₂ 传感器校准成功");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "校准响应无效");
        return ESP_FAIL;
    }
}
