/**
 * @file u8g2_esp32_hal.h
 * @brief u8g2 ESP32 HAL 适配层 - I2C 接口
 */

#ifndef U8G2_ESP32_HAL_H
#define U8G2_ESP32_HAL_H

#include "u8g2.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

/**
 * @brief u8g2 ESP32 HAL 配置结构
 */
typedef struct {
    i2c_port_t i2c_port;    ///< I2C 端口号
    gpio_num_t sda_pin;     ///< SDA 引脚
    gpio_num_t scl_pin;     ///< SCL 引脚
    uint32_t clk_speed;     ///< I2C 时钟速度（Hz）
} u8g2_esp32_hal_t;

/**
 * @brief 初始化 u8g2 ESP32 HAL
 * @param hal HAL 配置结构
 * @param u8g2 u8g2 对象指针
 */
void u8g2_esp32_hal_init(u8g2_esp32_hal_t hal, u8g2_t *u8g2);

/**
 * @brief I2C 字节传输回调函数
 */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/**
 * @brief GPIO 和延时回调函数
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif // U8G2_ESP32_HAL_H
