/**
 * @file u8g2_esp32_hal.c
 * @brief u8g2 ESP32 HAL 适配层实现
 */

#include "u8g2_esp32_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "U8G2_HAL";

// 全局 HAL 配置
static u8g2_esp32_hal_t g_hal_config;

void u8g2_esp32_hal_init(u8g2_esp32_hal_t hal, u8g2_t *u8g2) {
    g_hal_config = hal;

    // 注意：I2C 已由 SHT35 初始化，这里不再重复初始化
    ESP_LOGI(TAG, "u8g2 HAL 初始化完成 (I2C%d, SDA=%d, SCL=%d)",
             hal.i2c_port, hal.sda_pin, hal.scl_pin);
}

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[128];  // u8g2 最多发送 32 字节，128 足够
    static uint8_t buf_idx = 0;

    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            // I2C 初始化已由其他模块完成
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;

        case U8X8_MSG_BYTE_SEND: {
            uint8_t *data = (uint8_t *)arg_ptr;
            while (arg_int > 0) {
                buffer[buf_idx++] = *data;
                data++;
                arg_int--;
            }
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER: {
            // 发送 I2C 数据
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write(cmd, buffer, buf_idx, true);
            i2c_master_stop(cmd);

            esp_err_t ret = i2c_master_cmd_begin(g_hal_config.i2c_port, cmd, pdMS_TO_TICKS(100));
            i2c_cmd_link_delete(cmd);

            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "I2C 传输失败: %s", esp_err_to_name(ret));
                return 0;
            }
            break;
        }

        case U8X8_MSG_BYTE_SET_DC:
            // I2C 模式不需要 DC 引脚
            break;

        default:
            return 0;
    }

    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_DELAY_100NANO:
            // 延时 arg_int * 100 纳秒
            ets_delay_us(1);  // 最小延时 1 微秒
            break;

        case U8X8_MSG_DELAY_10MICRO:
            // 延时 arg_int * 10 微秒
            ets_delay_us(arg_int * 10);
            break;

        case U8X8_MSG_DELAY_MILLI:
            // 延时 arg_int 毫秒
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;

        case U8X8_MSG_DELAY_I2C:
            // I2C 时钟延时，arg_int 是速度（单位：100KHz）
            // 例如 arg_int=1 表示 100KHz，延时 5us
            // arg_int=4 表示 400KHz，延时 1.25us
            ets_delay_us(5);
            break;

        case U8X8_MSG_GPIO_I2C_CLOCK:
        case U8X8_MSG_GPIO_I2C_DATA:
            // 软件 I2C 模式才需要，硬件 I2C 忽略
            break;

        default:
            u8x8_SetGPIOResult(u8x8, 1);
            break;
    }

    return 1;
}
