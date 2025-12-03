# network-services Specification Delta

## MODIFIED Requirements

### Requirement: WiFi 连接管理

WiFi 管理模块 MUST 提供初始化、配网、连接检查和重连功能。

**实现状态变更**：从桩函数升级为完整实现，所有场景的行为保持不变。

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

**实现状态变更**：从桩函数升级为完整实现，所有场景的行为保持不变。

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

**实现状态变更**：从桩函数升级为完整实现，所有场景的行为保持不变。

#### Scenario: 初始化 MQTT 客户端

**Given** 系统配置中有 EMQX Cloud 连接信息（URL、用户名、密码）
**When** 调用 `mqtt_client_init()`
**Then**
- 创建 MQTT 客户端实例
- 配置 TLS 连接（启用 `esp_crt_bundle_attach`）
- 设置遗嘱消息（Last Will）：`home/ventilation/status` → `{"online": false}`
- 尝试连接到 Broker
- 函数返回 `ESP_OK`

#### Scenario: 发布设备状态（正常模式）

**Given** MQTT 已连接
**When** 调用 `mqtt_publish_status(&sensor_data, FAN_LOW, MODE_NORMAL)`
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
- 触发后台重连任务（通过定时器或独立任务，避免阻塞事件回调）
- 等待 30 秒后自动重连
- 重连成功后恢复正常发布

#### Scenario: WiFi 断开时 MQTT 发布失败

**Given** WiFi 未连接
**When** 调用 `mqtt_publish_status(&sensor_data, FAN_LOW)`
**Then**
- 检测到 MQTT 客户端未连接
- 函数返回 `ESP_FAIL`
- 不阻塞，立即返回
