/**
 * @file co2_sensor.c
 * @brief CO2 传感器驱动（MH-Z19B UART 接口）
 */

#include "co2_sensor.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"

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
    ESP_LOGI(TAG, "?? CO2 UART GPIO16/17 9600 8N1 ??");
    return ESP_OK;
}

float co2_sensor_read_ppm(void) {
    // TODO: ?? - ?? UART ?? ASCII ??
    // ????(??)
    return 850.0f;
}

bool co2_sensor_is_ready(void) {
    // TODO: ?? - ???????
    return true;
}

esp_err_t co2_sensor_calibrate(void) {
    ESP_LOGW(TAG, "????????");
    return ESP_ERR_NOT_SUPPORTED;
}
