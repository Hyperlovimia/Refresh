# 规范：网络服务

## 概述

本规范定义了网络子系统的接口和行为，包括 WiFi 连接管理、天气 API 客户端和 MQTT 客户端。

---

## ADDED Requirements

### Requirement: WiFi 连接管理

WiFi 管理模块 MUST 提供初始化、配网、连接检查和重连功能。

#### Scenario: 初始化 WiFi 管理器

**Given** ESP32-S3 WiFi 硬件未初始化
**When** 调用 `wifi_manager_init()`
**Then**
- WiFi 驱动初始化为 STA 模式（Station）
- 从 NVS 读取保存的 SSID/密码（如果存在）
- 如果 NVS 中有配置，自动尝试连接
- 函数返回 `ESP_OK`

#### Scenario: 首次配网（SmartConfig）

**Given** NVS 中无保存的 WiFi 配置
**When** 调用 `wifi_manager_start_provisioning()`
**Then**
- 进入 SmartConfig 模式
- 等待用户通过手机 APP 发送 WiFi 密码
- 接收到配置后自动连接
- 连接成功后保存 SSID/密码到 NVS
- 函数返回 `ESP_OK`

#### Scenario: 自动连接（已有配置）

**Given** NVS 中有保存的 WiFi 配置（SSID: "MyHome", Password: "12345678"）
**When** 调用 `wifi_manager_init()`
**Then**
- 读取 NVS 配置
- 自动尝试连接到 "MyHome"
- 连接成功后设置事件标志

#### Scenario: 连接失败重试

**Given** WiFi 连接失败（密码错误或信号不可达）
**When** `wifi_manager_init()` 检测到连接失败
**Then**
- 等待 5 秒
- 重试连接
- 最多重试 3 次
- 3 次失败后返回 `ESP_FAIL`

#### Scenario: 连接断开后自动重连

**Given** WiFi 已连接并正常工作
**When** 路由器断电导致连接断开
**Then**
- 检测到断开事件
- 每 10 秒自动重试连接
- 无限次重试直到连接成功

#### Scenario: 检查连接状态

**Given** WiFi 当前已连接
**When** 调用 `wifi_manager_is_connected()`
**Then** 返回 `true`

**Given** WiFi 当前未连接
**When** 调用 `wifi_manager_is_connected()`
**Then** 返回 `false`

---

### Requirement: 天气 API 客户端

天气 API 客户端 MUST 提供初始化、数据获取、缓存管理功能。

#### Scenario: 初始化天气 API 客户端

**Given** 系统配置中有和风天气 API Key 和城市代码
**When** 调用 `weather_api_init()`
**Then**
- 从 menuconfig 读取 API Key 和城市代码
- 初始化 HTTPS 客户端
- 清空缓存
- 函数返回 `ESP_OK`

#### Scenario: 获取实时天气数据（成功）

**Given** WiFi 已连接且网络正常
**When** 调用 `weather_api_fetch(&data)`
**Then**
- 发送 HTTPS GET 请求到 `https://api.qweather.com/v7/air/now?location=城市代码&key=API_KEY`
- 解析 JSON 响应：
  ```json
  {
    "now": {
      "pm2p5": "35",
      "temp": "22",
      "windSpeed": "12"
    }
  }
  ```
- 填充 `WeatherData` 结构体：
  ```c
  data.pm25 = 35.0f;
  data.temperature = 22.0f;
  data.wind_speed = 12.0f;
  data.timestamp = current_time;
  data.valid = true;
  ```
- 保存到缓存
- 函数返回 `ESP_OK`

#### Scenario: 获取天气数据（网络失败）

**Given** WiFi 未连接或 API 服务器不可达
**When** 调用 `weather_api_fetch(&data)`
**Then**
- HTTPS 请求超时（10秒）
- 函数返回 `ESP_FAIL`
- 缓存数据不变

#### Scenario: 使用缓存数据（缓存有效）

**Given** 上次成功获取天气数据的时间为 15 分钟前
**When** 调用 `weather_api_get_cached(&data)`
**Then**
- 从缓存中读取数据
- 填充 `WeatherData` 结构体
- 缓存标记为有效（`data.valid = true`）

#### Scenario: 检查缓存是否过期

**Given** 上次成功获取天气数据的时间为 40 分钟前
**When** 调用 `weather_api_is_cache_stale()`
**Then** 返回 `true`（缓存已过期）

**Given** 上次成功获取天气数据的时间为 10 分钟前
**When** 调用 `weather_api_is_cache_stale()`
**Then** 返回 `false`（缓存仍有效）

---

### Requirement: MQTT 客户端

MQTT 客户端 MUST 提供初始化、状态发布、告警发布功能。

#### Scenario: 初始化 MQTT 客户端

**Given** 系统配置中有 EMQX Cloud 连接信息（URL、用户名、密码）
**When** 调用 `mqtt_client_init()`
**Then**
- 创建 MQTT 客户端实例
- 配置 TLS 连接（启用 `esp_crt_bundle_attach`）
- 设置遗嘱消息（Last Will）：`home/ventilation/status` → `{"online": false}`
- 尝试连接到 Broker
- 连接成功后订阅 `home/ventilation/command`（预留远程控制）
- 函数返回 `ESP_OK`

#### Scenario: 发布设备状态（正常模式）

**Given** MQTT 已连接
**When** 调用 `mqtt_publish_status(&sensor_data, FAN_LOW)`
**Then**
- 构建 JSON 消息：
  ```json
  {
    "co2": 850,
    "temp": 24.5,
    "humi": 58,
    "fan_state": "LOW",
    "mode": "NORMAL",
    "timestamp": 1700000000
  }
  ```
- 发布到主题 `home/ventilation/status`，QoS 0
- 函数返回 `ESP_OK`

#### Scenario: 发布告警消息

**Given** CO₂ 浓度超过 1500ppm
**When** 调用 `mqtt_publish_alert("CO₂过高：1520ppm")`
**Then**
- 构建 JSON 消息：
  ```json
  {
    "alert": "CO₂过高：1520ppm",
    "level": "WARNING",
    "timestamp": 1700000000
  }
  ```
- 发布到主题 `home/ventilation/alert`，QoS 1（保证送达）
- 函数返回 `ESP_OK`

#### Scenario: MQTT 断开后自动重连

**Given** MQTT 已连接
**When** Broker 重启导致连接断开
**Then**
- 检测到断开事件
- 等待 30 秒
- 自动重连
- 重连成功后重新订阅主题

#### Scenario: WiFi 断开时 MQTT 发布失败

**Given** WiFi 未连接
**When** 调用 `mqtt_publish_status(&sensor_data, FAN_LOW)`
**Then**
- 检测到 MQTT 客户端未连接
- 函数返回 `ESP_FAIL`
- 不阻塞，立即返回

---

## 数据结构定义

```c
// 天气数据结构
typedef struct {
    float pm25;         // PM2.5 浓度（μg/m³）
    float temperature;  // 室外温度（°C）
    float wind_speed;   // 风速（m/s）
    time_t timestamp;   // 数据时间戳
    bool valid;         // 数据是否有效
} WeatherData;
```

---

## 接口定义

### WiFi 管理（network/wifi_manager.h）

```c
/**
 * @brief 初始化 WiFi 管理器（自动连接已保存的配置）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 启动 WiFi 配网（SmartConfig）
 * @return ESP_OK 成功, ESP_FAIL 超时或失败
 */
esp_err_t wifi_manager_start_provisioning(void);

/**
 * @brief 检查 WiFi 连接状态
 * @return true 已连接, false 未连接
 */
bool wifi_manager_is_connected(void);
```

### 天气 API 客户端（network/weather_api.h）

```c
/**
 * @brief 初始化天气 API 客户端
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t weather_api_init(void);

/**
 * @brief 获取实时天气数据
 * @param data 输出参数，天气数据结构
 * @return ESP_OK 成功, ESP_FAIL 网络失败或解析失败
 */
esp_err_t weather_api_fetch(WeatherData *data);

/**
 * @brief 获取缓存的天气数据
 * @param data 输出参数，天气数据结构
 */
void weather_api_get_cached(WeatherData *data);

/**
 * @brief 检查缓存是否过期（>30分钟）
 * @return true 过期, false 有效
 */
bool weather_api_is_cache_stale(void);
```

### MQTT 客户端（network/mqtt_client.h）

```c
/**
 * @brief 初始化 MQTT 客户端（连接到 Broker）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t mqtt_client_init(void);

/**
 * @brief 发布设备状态
 * @param sensor 传感器数据
 * @param fan 风扇状态
 * @return ESP_OK 成功, ESP_FAIL 未连接或发布失败
 */
esp_err_t mqtt_publish_status(SensorData *sensor, FanState fan);

/**
 * @brief 发布告警消息
 * @param message 告警内容
 * @return ESP_OK 成功, ESP_FAIL 未连接或发布失败
 */
esp_err_t mqtt_publish_alert(const char *message);
```

---

## 约束条件

1. **WiFi 配置**：
   - 支持 SmartConfig（ESP-Touch）配网
   - SSID/密码保存到 NVS
   - 自动重连：连接失败每 5 秒重试 3 次，断开后每 10 秒无限重试

2. **天气 API**：
   - 服务商：和风天气（https://api.qweather.com）
   - 缓存有效期：30 分钟
   - 请求超时：10 秒
   - API Key 和城市代码通过 menuconfig 配置

3. **MQTT**：
   - Broker：EMQX Cloud（mqtts://xxx.emqxsl.cn:8883）
   - TLS：启用，使用 `esp_crt_bundle_attach` 验证证书
   - QoS：状态消息 QoS 0，告警消息 QoS 1
   - 重连间隔：30 秒

4. **网络降级策略**：
   - WiFi 正常 → MODE_NORMAL
   - WiFi 断开 + 缓存有效（<30min）→ MODE_DEGRADED
   - WiFi 断开 + 缓存过期（>30min）→ MODE_LOCAL

---

## 测试要求

1. **单元测试**：
   - JSON 解析正确性（天气 API 响应）
   - 缓存过期检测逻辑
   - MQTT 消息构建正确性

2. **集成测试**：
   - WiFi 连接和断开场景
   - 天气 API 请求成功和失败场景
   - MQTT 发布和订阅功能

3. **压力测试**：
   - 网络频繁断开/重连（10 次/小时，持续 24 小时）
   - MQTT 消息大量发布（100 条/分钟，持续 1 小时）

---

## 安全性

1. **TLS/HTTPS**：
   - 天气 API 使用 HTTPS
   - MQTT 使用 TLS（mqtts://）
   - 证书验证：启用 `esp_crt_bundle_attach`

2. **密钥管理**：
   - API Key 和 MQTT 密码通过 menuconfig 配置
   - WiFi 密码加密保存到 NVS（可选启用 NVS 加密）

3. **输入验证**：
   - 天气 API 响应的数值范围检查（PM2.5: 0-500，温度: -40 - 60，风速: 0-50）
