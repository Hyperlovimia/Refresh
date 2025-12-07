# network-services Spec Delta

## MODIFIED Requirements

### Requirement: MQTT 命令订阅

MQTT 客户端 MUST 支持接收3个风扇的独立控制命令。

**原规格变更点**：
- 命令格式从单风扇扩展为扁平式多风扇格式
- 内部缓冲区从单值扩展为数组
- 获取接口返回3个风扇状态

#### Scenario: 接收多风扇控制命令（完整格式）

**Given**：
- MQTT 已连接并订阅命令主题

**When**：
- 收到消息：
```json
{
  "fan_0": "HIGH",
  "fan_1": "LOW",
  "fan_2": "OFF"
}
```

**Then**：
- 解析 JSON 获取 `fan_0`, `fan_1`, `fan_2` 字段
- 转换为 FanState 数组：`[FAN_HIGH, FAN_LOW, FAN_OFF]`
- 存储到内部命令缓冲区
- 日志记录收到的命令

**Acceptance Criteria**：
- 3个风扇状态正确解析
- 字段名为 `fan_0`, `fan_1`, `fan_2`
- 值支持 "OFF", "LOW", "HIGH"

#### Scenario: 接收部分风扇控制命令

**Given**：
- MQTT 已连接，当前缓冲区为 `[FAN_OFF, FAN_OFF, FAN_OFF]`

**When**：
- 收到消息：
```json
{
  "fan_1": "HIGH"
}
```

**Then**：
- 仅更新 fan_1 为 FAN_HIGH
- fan_0 和 fan_2 保持原值
- 缓冲区变为 `[FAN_OFF, FAN_HIGH, FAN_OFF]`

**Acceptance Criteria**：
- 支持部分更新
- 未指定的风扇状态保持不变

#### Scenario: 获取最新远程命令（多风扇）

**Given**：
- 已收到远程命令 `{"fan_0": "LOW", "fan_1": "HIGH", "fan_2": "OFF"}`

**When**：
- 调用 `mqtt_get_remote_command(cmd_array)`

**Then**：
- `cmd_array[0]` = FAN_LOW
- `cmd_array[1]` = FAN_HIGH
- `cmd_array[2]` = FAN_OFF
- 函数返回 `true`

**Acceptance Criteria**：
- 输出数组长度为 FAN_COUNT (3)
- 状态值正确映射

---

### Requirement: MQTT 客户端

MQTT 客户端 MUST 在状态上报中包含3个风扇的独立状态。

**原规格变更点**：
- 状态JSON从单个fan_state扩展为fan_0/fan_1/fan_2

#### Scenario: 发布多风扇设备状态

**Given**：
- MQTT 已连接
- 传感器数据有效
- 风扇状态为 `[FAN_LOW, FAN_HIGH, FAN_OFF]`

**When**：
- 调用 `mqtt_publish_status(&sensor_data, fan_states, MODE_REMOTE)`

**Then**：
- 构建 JSON 消息：
```json
{
  "co2": 850,
  "temp": 24.5,
  "humi": 58,
  "fan_0": "LOW",
  "fan_1": "HIGH",
  "fan_2": "OFF",
  "mode": "REMOTE",
  "timestamp": 1733500000
}
```
- 发布到主题 `home/ventilation/status`，QoS 0
- 函数返回 `ESP_OK`

**Acceptance Criteria**：
- JSON 包含 fan_0, fan_1, fan_2 三个字段
- 状态值为字符串 "OFF"/"LOW"/"HIGH"
- 其他字段（co2, temp, humi, mode, timestamp）保持不变

#### Scenario: 遗嘱消息格式

**Given**：
- MQTT 客户端初始化

**When**：
- 配置遗嘱消息

**Then**：
- 遗嘱消息格式更新为：
```json
{
  "online": false
}
```

**Acceptance Criteria**：
- 遗嘱消息保持简洁
- 不包含风扇状态（设备离线时状态无意义）
