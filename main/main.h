/**
 * @file main.h
 * @brief 智能室内空气质量改善系统 - 全局数据结构和类型定义
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ============================================================================
// 系统状态枚举
// ============================================================================

/**
 * @brief 系统运行状态
 */
typedef enum {
    STATE_INIT,         ///< 初始化状态（模块启动）
    STATE_PREHEATING,   ///< 预热状态（60秒，CO?传感器预热）
    STATE_STABILIZING,  ///< 稳定状态（240秒，CO?传感器稳定）
    STATE_RUNNING,      ///< 正常运行状态
    STATE_ERROR         ///< 错误状态（传感器故障，安全停机）
} SystemState;

/**
 * @brief 系统运行模式
 */
typedef enum {
    MODE_NORMAL,    ///< 正常模式（WiFi + 天气数据 + 完整算法）
    MODE_DEGRADED,  ///< 降级模式（WiFi 断开但缓存有效）
    MODE_LOCAL,     ///< 本地模式（网络离线 >30min，仅 CO? 决策）
    MODE_SAFE_STOP  ///< 安全停机（传感器故障）
} SystemMode;

/**
 * @brief 风扇状态枚举
 */
typedef enum {
    FAN_OFF = 0,    ///< 风扇关闭
    FAN_LOW = 1,    ///< 低速档位
    FAN_HIGH = 2,   ///< 高速档位
} FanState;

// ============================================================================
// 数据结构定义
// ============================================================================

/**
 * @brief 传感器数据结构
 */
typedef struct {
    float co2;          ///< CO? 浓度（ppm）
    float temperature;  ///< 温度（摄氏度）
    float humidity;     ///< 相对湿度（%）
    time_t timestamp;   ///< 数据时间戳
    bool valid;         ///< 数据有效标志
} SensorData;

/**
 * @brief 天气数据结构
 */
typedef struct {
    float pm25;         ///< PM2.5 浓度（μg/m?）
    float temperature;  ///< 室外温度（摄氏度）
    float wind_speed;   ///< 风速（km/h）
    time_t timestamp;   ///< 数据时间戳
    bool valid;         ///< 数据有效标志
} WeatherData;

// ============================================================================
// 系统常量定义
// ============================================================================

// 预热和稳定时间常量
#define PREHEATING_TIME_SEC     60      ///< CO? 传感器预热时间（秒）
#define STABILIZING_TIME_SEC    240     ///< CO? 传感器稳定时间（秒）

// 决策阈值常量
#define CO2_THRESHOLD_LOW       1000.0f ///< CO? 低速阈值（ppm）
#define CO2_THRESHOLD_HIGH      1200.0f ///< CO? 高速阈值（ppm）
#define CO2_ALERT_THRESHOLD     1500.0f ///< CO? 告警阈值（ppm）

// 数据有效性范围
#define CO2_MIN_VALID           300.0f  ///< CO? 最小有效值（ppm）
#define CO2_MAX_VALID           5000.0f ///< CO? 最大有效值（ppm）
#define TEMP_MIN_VALID          -10.0f  ///< 温度最小有效值（℃）
#define TEMP_MAX_VALID          50.0f   ///< 温度最大有效值（℃）
#define HUMI_MIN_VALID          0.0f    ///< 湿度最小有效值（%）
#define HUMI_MAX_VALID          100.0f  ///< 湿度最大有效值（%）

// 网络和缓存常量
#define WEATHER_CACHE_VALID_SEC 1800    ///< 天气数据缓存有效期（秒，30分钟）
#define MQTT_PUBLISH_INTERVAL_SEC 30    ///< MQTT 状态上报间隔（秒）
#define WEATHER_FETCH_INTERVAL_SEC 600  ///< 天气数据获取间隔（秒，10分钟）

// 任务优先级定义
#define TASK_PRIORITY_MAIN      4       ///< 主任务优先级（最高）
#define TASK_PRIORITY_SENSOR    3       ///< 传感器任务优先级
#define TASK_PRIORITY_DECISION  3       ///< 决策任务优先级
#define TASK_PRIORITY_NETWORK   2       ///< 网络任务优先级
#define TASK_PRIORITY_DISPLAY   2       ///< 显示任务优先级

// 任务栈大小定义
#define TASK_STACK_SIZE_SMALL   4096    ///< 小栈（4KB）
#define TASK_STACK_SIZE_LARGE   8192    ///< 大栈（8KB）

// ============================================================================
// 全局函数声明
// ============================================================================

/**
 * @brief 获取 I2C 互斥锁（用于保护 I2C 总线访问）
 * @return I2C 互斥锁句柄
 */
SemaphoreHandle_t get_i2c_mutex(void);

#endif // MAIN_H
