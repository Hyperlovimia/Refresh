# network-services Specification

## Purpose
TBD - created by archiving change implement-air-quality-system. Update Purpose after archive.
## Requirements
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

### Requirement: MQTT 客户端

MQTT 客户端 MUST 提供初始化、状态发布、告警发布和命令订阅功能。

**变更说明**: 新增命令订阅功能，用于接收远程服务器的风扇控制命令。

#### Scenario: 初始化 MQTT 客户端（保持不变）

**Given** 系统配置中有 EMQX Cloud 连接信息
**When** 调用 `mqtt_client_init()`
**Then**
- 创建 MQTT 客户端实例
- 配置 TLS 连接
- 设置遗嘱消息
- 尝试连接到 Broker
- 函数返回 `ESP_OK`

#### Scenario: 发布设备状态（保持不变）

**Given** MQTT 已连接
**When** 调用 `mqtt_publish_status(&sensor_data, FAN_LOW, MODE_REMOTE)`
**Then**
- 构建 JSON 消息
- 发布到主题 `home/ventilation/status`，QoS 0
- 函数返回 `ESP_OK`

---

### Requirement: MQTT 命令订阅

MQTT 客户端 MUST 订阅远程控制命令主题，并提供命令获取接口。

#### Scenario: 连接成功后订阅命令主题

**Given** MQTT 连接成功
**When** 收到 `MQTT_EVENT_CONNECTED` 事件
**Then**
- 自动订阅主题 `home/ventilation/command`，QoS 1
- 日志记录订阅成功

#### Scenario: 接收风扇控制命令

**Given** MQTT 已连接并订阅命令主题
**When** 收到消息：
```json
{
  "fan_state": "HIGH"
}
```
**Then**
- 解析 JSON 获取 `fan_state` 字段
- 转换为 `FanState` 枚举值
- 存储到内部缓冲区
- 日志记录收到的命令

#### Scenario: 获取最新远程命令

**Given** 已收到远程命令 `{"fan_state": "LOW"}`
**When** 调用 `mqtt_get_remote_command(&fan_state)`
**Then**
- `fan_state` 被设置为 `FAN_LOW`
- 函数返回 `true`

#### Scenario: 无有效远程命令

**Given** 未收到任何远程命令
**When** 调用 `mqtt_get_remote_command(&fan_state)`
**Then**
- `fan_state` 不被修改
- 函数返回 `false`

#### Scenario: 命令格式错误

**Given** MQTT 已连接
**When** 收到无效消息 `{"invalid": "data"}`
**Then**
- 日志记录解析错误
- 不更新内部命令缓冲区

---

### Requirement: 传感器数据有效性检查

MQTT 状态发布 MUST 仅在传感器数据有效时执行。

#### Scenario: 传感器数据有效时发布

**Given** `sensor->valid == true`
**When** 调用 `mqtt_publish_status(&sensor, fan, mode)`
**Then** 正常发布状态消息

#### Scenario: 传感器数据无效时跳过发布

**Given** `sensor->valid == false`
**When** 调用 `mqtt_publish_status(&sensor, fan, mode)`
**Then**
- 日志记录跳过原因
- 函数返回 `ESP_FAIL`

