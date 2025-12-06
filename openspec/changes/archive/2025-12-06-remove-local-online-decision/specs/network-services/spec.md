# network-services Spec Delta

## REMOVED Requirements

### Requirement: 天气 API 客户端

天气 API 客户端模块将被删除。

**移除原因**: 天气数据获取迁移至远程服务器，本地设备不再直接调用天气 API。

---

## MODIFIED Requirements

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

## ADDED Requirements

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
