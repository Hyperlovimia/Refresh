/**
 * @file i2c_scanner.c
 * @brief 临时 I2C 扫描任务，用于调试总线上的设备地址
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCANNER";

void i2c_scanner_task(void *pv) {
    ESP_LOGI(TAG, "开始 I2C 扫描 (I2C_NUM_0)");
    for (int addr = 0x03; addr <= 0x77; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "I2C 扫描完成");

    // 任务完成后删除自己
    vTaskDelete(NULL);
}
