/**
 * @file weather_api.h
 * @brief ŒÎ) API ¢7ï¥ã
 */

#ifndef WEATHER_API_H
#define WEATHER_API_H

#include "esp_err.h"
#include "main.h"
#include <stdbool.h>

/**
 * @brief Ë) API ¢7ï
 *
 * Ë HTTPS ¢7ïŒX
 *
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t weather_api_init(void);

/**
 * @brief ·Öö)pnÇ HTTPS ÷B	
 *
 * ÷BŒÎ) APIã JSON Í”ĞÖ PM2.5)¦Î
 *
 * @param data )pnˆ
 * @return ESP_OK ŸESP_FAIL 1%
 */
esp_err_t weather_api_fetch(WeatherData *data);

/**
 * @brief ·ÖX„)pn
 *
 * @param data )pnˆ
 */
void weather_api_get_cached(WeatherData *data);

/**
 * @brief ÀåX/&Ç
 *
 * X	H30Ÿ
 *
 * @return true Çfalse 	H
 */
bool weather_api_is_cache_stale(void);

#endif // WEATHER_API_H
