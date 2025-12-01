/**
 * @file sht35.c
 * @brief SHT35 温度湿度传感器驱动
 */

#include "sht35.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char *TAG = "SHT35";
static bool s_i2c_ready = false;

#define SHT35_I2C_NUM           I2C_NUM_0
#define SHT35_I2C_ADDR          0x44
#define SHT35_I2C_SDA           GPIO_NUM_21
#define SHT35_I2C_SCL           GPIO_NUM_20
#define SHT35_I2C_FREQ_HZ       100000

esp_err_t sht35_init(void) {
    if (s_i2c_ready) {
        return ESP_OK;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SHT35_I2C_SDA,
        .scl_io_num = SHT35_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = SHT35_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(SHT35_I2C_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config 失败 (%d)", err);
        return err;
    }

    err = i2c_driver_install(SHT35_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install 失败 (%d)", err);
        return err;
    }

    s_i2c_ready = true;
    ESP_LOGI(TAG, "初始化 SHT35 完成 I2C GPIO21/20 100kHz 地址0x44");
    return ESP_OK;
}

/**
 * @brief 计算 CRC-8 校验和（SHT35 多项式：0x31，初值：0xFF）
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC-8 校验值
 */
static uint8_t crc8_compute(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

esp_err_t sht35_read(float *temp, float *humi) {
    if (!s_i2c_ready) {
        ESP_LOGE(TAG, "I2C 未初始化");
        return ESP_FAIL;
    }

    if (!temp || !humi) {
        return ESP_ERR_INVALID_ARG;
    }

    // 发送单次测量命令（高重复性）：0x2C 0x06
    uint8_t cmd[2] = {0x2C, 0x06};
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SHT35_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd, 2, true);
    i2c_master_stop(cmd_handle);
    esp_err_t err = i2c_master_cmd_begin(SHT35_I2C_NUM, cmd_handle, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发送测量命令失败 (%d)", err);
        return err;
    }

    // 延迟 15ms 等待测量完成
    vTaskDelay(pdMS_TO_TICKS(15));

    // 读取 6 字节数据
    uint8_t data[6];
    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SHT35_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);
    err = i2c_master_cmd_begin(SHT35_I2C_NUM, cmd_handle, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "读取数据失败 (%d)", err);
        return err;
    }

    // 验证温度 CRC
    uint8_t temp_crc = crc8_compute(data, 2);
    if (temp_crc != data[2]) {
        ESP_LOGW(TAG, "温度 CRC 校验失败 (计算:%02X, 接收:%02X)", temp_crc, data[2]);
        return ESP_ERR_INVALID_CRC;
    }

    // 验证湿度 CRC
    uint8_t humi_crc = crc8_compute(data + 3, 2);
    if (humi_crc != data[5]) {
        ESP_LOGW(TAG, "湿度 CRC 校验失败 (计算:%02X, 接收:%02X)", humi_crc, data[5]);
        return ESP_ERR_INVALID_CRC;
    }

    // 转换原始值到物理值
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_humi = (data[3] << 8) | data[4];

    *temp = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humi = 100.0f * ((float)raw_humi / 65535.0f);

    return ESP_OK;
}
