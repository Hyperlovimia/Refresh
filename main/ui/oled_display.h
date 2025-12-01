/**
 * @file oled_display.h
 * @brief OLED >:Oq¨¥ãSSD1306I2C	
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "esp_err.h"
#include "main.h"

/**
 * @brief Ë OLED >:O
 *
 * Ë u8g2 “Œ I2C SHT35 q« GPIO21/22	
 * ¨‡128×64 Ur
 * Î¾0@0x3C7M0@	
 *
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t oled_display_init(void);

/**
 * @brief >:;ub
 *
 * @
 *   - ,1LCO‚)¦¦žöpn
 *   - ,2-3LCO‚ ‹¿þ˜¿þ	
 *   - ,4L¶ûß!ÎG¶WiFi¶	
 *
 * @param sensor  hpnˆ
 * @param weather )pnˆ
 * @param fan ÎG¶
 * @param mode ûßÐL!
 */
void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode);

/**
 * @brief >:Jfub
 *
 * (Ž>:ïJfˆo‚" hEœ"	
 *
 * @param message Jfˆo
 */
void oled_display_alert(const char *message);

/**
 * @brief û  CO‚ ‹¿þpn¹„YŸý	
 *
 * @param co2 CO‚ S¦ppm	
 */
void oled_add_history_point(float co2);

#endif // OLED_DISPLAY_H
