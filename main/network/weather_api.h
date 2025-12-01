/**
 * @file weather_api.h
 * @brief 天气 API 客户端接口定义
 */

#ifndef WEATHER_API_H
#define WEATHER_API_H

#include "esp_err.h"
#include "main.h"
#include <stdbool.h>

/**
 * @brief 初始化天气 API 客户端
 * 初始化 HTTPS 客户端和 API Key
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t weather_api_init(void);

/**
 * @brief 获取天气数据
 * HTTPS 请求 API，解析 JSON 获取 PM2.5 等数据
 * @param data 输出天气数据
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t weather_api_fetch(WeatherData *data);

/**
 * @brief 获取缓存的天气数据
 * @param data 输出天气数据
 */
void weather_api_get_cached(WeatherData *data);

/**
 * @brief 检查缓存是否过期
 * @return true 过期，false 有效
 */
bool weather_api_is_cache_stale(void);

#endif // WEATHER_API_H
