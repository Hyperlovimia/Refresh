/**
 * @file local_mode.c
 * @brief ?????????
 */

#include "local_mode.h"
#include "esp_log.h"

static const char *TAG = "LOCAL_MODE";

FanState local_mode_decide(float co2_ppm) {
    ESP_LOGD(TAG, "???????CO2 = %.1f ppm", co2_ppm);

    // ????????
    if (co2_ppm > CO2_THRESHOLD_HIGH) {
        return FAN_HIGH;
    } else if (co2_ppm > CO2_THRESHOLD_LOW) {
        return FAN_LOW;
    } else {
        return FAN_OFF;
    }
}
