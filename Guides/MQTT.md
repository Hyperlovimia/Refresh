# MQTT 消息格式与数据流向说明

## 一、MQTT 主题定义

系统使用 3 个 MQTT 主题进行通信：

| 主题 | 方向 | 用途 | QoS |
|------|------|------|-----|
| `home/ventilation/status` | 设备 → 服务器 | 定期上报设备状态 | 0 |
| `home/ventilation/alert` | 设备 → 服务器 | 发送告警消息 | 1 |
| `home/ventilation/command` | 服务器 → 设备 | 接收远程控制命令 | 1 |

---

## 二、消息格式详解

### 2.1 状态上报消息（status）

**主题**: `home/ventilation/status`  
**方向**: 设备 → 服务器  
**频率**: 每 30 秒一次（由 `MQTT_PUBLISH_INTERVAL_SEC` 定义）  
**QoS**: 0（不保证送达）

**JSON 格式**:
```json
{
  "co2": 850,
  "temp": 24.5,
  "humi": 58,
  "fan_state": "LOW",
  "mode": "REMOTE",
  "timestamp": 1701936000
}
```

**字段说明**:
- `co2`: CO₂ 浓度（ppm），浮点数
- `temp`: 温度（℃），浮点数
- `humi`: 湿度（%），浮点数
- `fan_state`: 风扇状态，字符串枚举值：
  - `"OFF"` - 关闭
  - `"LOW"` - 低速
  - `"HIGH"` - 高速
- `mode`: 系统运行模式，字符串枚举值：
  - `"REMOTE"` - 远程控制模式（WiFi 已连接）
  - `"LOCAL"` - 本地自动模式（WiFi 未连接）
  - `"SAFE_STOP"` - 安全停机模式（传感器故障）
- `timestamp`: Unix 时间戳（秒）

---

### 2.2 告警消息（alert）

**主题**: `home/ventilation/alert`  
**方向**: 设备 → 服务器  
**触发条件**: 
- CO₂ 浓度超过 1500 ppm
- 传感器故障检测
**QoS**: 1（保证至少送达一次）

**JSON 格式**:
```json
{
  "alert": "CO₂浓度过高: 1650 ppm",
  "level": "WARNING",
  "timestamp": 1701936000
}
```

**字段说明**:
- `alert`: 告警消息内容（中文）
- `level`: 告警级别，固定为 `"WARNING"`
- `timestamp`: Unix 时间戳（秒）

---

### 2.3 远程控制命令（command）

**主题**: `home/ventilation/command`  
**方向**: 服务器 → 设备  
**QoS**: 1（保证至少送达一次）

**JSON 格式**:
```json
{
  "fan_state": "HIGH"
}
```

**字段说明**:
- `fan_state`: 目标风扇状态，字符串枚举值：
  - `"OFF"` - 关闭风扇
  - `"LOW"` - 设置为低速
  - `"HIGH"` - 设置为高速

**注意事项**:
- 仅在系统处于 `MODE_REMOTE` 模式时生效
- 命令会被缓存，决策任务会定期读取最新命令
- 如果 WiFi 断开，系统会自动切换到 `MODE_LOCAL` 模式，忽略远程命令

---

## 三、本地代码数据流向

### 3.1 远程命令接收流程

```
MQTT Broker (EMQX Cloud)
    ↓ (订阅 home/ventilation/command)
mqtt_event_handler() [mqtt_wrapper.c:115]
    ↓ (MQTT_EVENT_DATA 事件)
parse_remote_command() [mqtt_wrapper.c:82]
    ↓ (解析 JSON，提取 fan_state)
s_remote_command (全局变量缓存)
    ↓ (决策任务定期轮询)
mqtt_get_remote_command() [mqtt_wrapper.c:264]
    ↓ (返回缓存的命令)
decision_task() [main.c:267]
    ↓ (仅在 MODE_REMOTE 时使用)
decision_make() [decision_engine.c:11]
    ↓ (返回目标风扇状态)
fan_control_set_state() [fan_control.c]
    ↓ (执行 PWM 控制)
风扇硬件
```

**关键代码位置**:
1. **MQTT 事件处理**: `mqtt_wrapper.c:115` - `mqtt_event_handler()`
2. **JSON 解析**: `mqtt_wrapper.c:82` - `parse_remote_command()`
3. **命令缓存**: `mqtt_wrapper.c:35` - `s_remote_command` 全局变量
4. **命令读取**: `mqtt_wrapper.c:264` - `mqtt_get_remote_command()`
5. **决策逻辑**: `main.c:267` - `decision_task()`

---

### 3.2 状态上报流程

```
传感器硬件
    ↓ (I2C 读取)
sensor_task() [main.c:195]
    ↓ (1Hz 周期读取)
shared_sensor_data (共享缓冲区)
    ↓ (互斥锁保护)
network_task() [main.c:313]
    ↓ (30秒周期检查)
mqtt_publish_status() [mqtt_wrapper.c:177]
    ↓ (构建 JSON，QoS=0)
esp_mqtt_client_publish()
    ↓ (TLS 加密传输)
MQTT Broker (EMQX Cloud)
```

**关键代码位置**:
1. **传感器读取**: `main.c:195` - `sensor_task()`
2. **数据共享**: `main.c:207` - `shared_sensor_data` 全局变量
3. **定期上报**: `main.c:313` - `network_task()`
4. **JSON 构建**: `mqtt_wrapper.c:177` - `mqtt_publish_status()`

---

### 3.3 告警发送流程

```
传感器数据 (CO₂ > 1500 ppm)
    ↓
sensor_task() [main.c:217]
    ↓ (检测阈值)
alert_queue (FreeRTOS 队列)
    ↓ (异步传递)
display_task() [main.c:356] (显示告警)
    +
mqtt_publish_alert() [mqtt_wrapper.c:223] (发送到服务器)
    ↓ (构建 JSON，QoS=1)
esp_mqtt_client_publish()
    ↓ (TLS 加密传输)
MQTT Broker (EMQX Cloud)
```

**关键代码位置**:
1. **阈值检测**: `main.c:217` - `sensor_task()` 中的 CO₂ 告警逻辑
2. **告警队列**: `main.c:217` - `xQueueSend(alert_queue, ...)`
3. **MQTT 发送**: `mqtt_wrapper.c:223` - `mqtt_publish_alert()`

---

## 四、系统模式切换逻辑

系统根据 WiFi 连接状态和传感器健康状态自动切换运行模式：

```c
// decision_engine.c:23
SystemMode decision_detect_mode(bool wifi_ok, bool sensor_ok) {
    if (!sensor_ok) {
        return MODE_SAFE_STOP;  // 传感器故障 → 安全停机
    }
    if (wifi_ok) {
        return MODE_REMOTE;     // WiFi 已连接 → 远程控制
    }
    return MODE_LOCAL;          // WiFi 未连接 → 本地自动
}
```

**模式优先级**:
1. **MODE_SAFE_STOP** (最高优先级) - 传感器故障时强制关闭风扇
2. **MODE_REMOTE** - WiFi 已连接时接受远程命令
3. **MODE_LOCAL** - WiFi 未连接时根据 CO₂ 浓度自动决策

---

## 五、MQTT 连接配置

**Broker 信息**:
- URL: `mqtts://xxx.emqxsl.cn:8883` (从 `CONFIG_MQTT_BROKER_URL` 读取)
- 协议: MQTT over TLS (使用 ESP-IDF 内置证书包)
- 认证: 用户名/密码 (从 `CONFIG_MQTT_USERNAME` 和 `CONFIG_MQTT_PASSWORD` 读取)

**Client ID 生成**:
```c
// mqtt_wrapper.c:138
esp_read_mac(mac, ESP_MAC_WIFI_STA);
snprintf(client_id, sizeof(client_id), "esp32_%02x%02x%02x%02x%02x%02x",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
```
示例: `esp32_a4b1c2d3e4f5`

**遗嘱消息（Last Will）**:
- 主题: `home/ventilation/status`
- 内容: `{"online": false}`
- 用途: 设备异常掉线时自动发送

**重连机制**:
- 断开连接后 30 秒自动重连（由 FreeRTOS 定时器触发）
- 重连逻辑: `mqtt_wrapper.c:103` - `reconnect_timer_callback()`

---

## 六、数据同步与线程安全

系统使用 FreeRTOS 同步机制保证多任务数据安全：

| 共享资源 | 保护机制 | 访问任务 |
|---------|---------|---------|
| `shared_sensor_data` | `data_mutex` (互斥锁) | sensor_task, decision_task, network_task, display_task |
| `shared_fan_state` | `data_mutex` (互斥锁) | decision_task, network_task, display_task |
| `s_remote_command` | 无锁（单写多读） | mqtt_event_handler (写), decision_task (读) |
| `alert_queue` | FreeRTOS 队列 | sensor_task (写), display_task (读) |

**注意**: `s_remote_command` 使用原子操作（单字节枚举），无需互斥锁保护。

---

## 七、调试与监控

### 7.1 日志标签
- `MQTT_CLIENT` - MQTT 客户端相关日志
- `DECISION` - 决策引擎日志
- `MAIN` - 主程序状态机日志

### 7.2 关键日志示例

**远程命令接收**:
```
I (12345) MQTT_CLIENT: 收到远程命令: fan_state=HIGH
I (12346) DECISION: 远程模式: fan_state=2
```

**状态上报**:
```
I (12347) MQTT_CLIENT: 发布状态: {"co2":850,"temp":24.5,"humi":58,"fan_state":"LOW","mode":"REMOTE","timestamp":1701936000} (msg_id=123)
```

**MQTT 重连**:
```
W (12348) MQTT_CLIENT: MQTT 连接断开
I (12349) MQTT_CLIENT: 将在 30 秒后重连 MQTT...
I (42349) MQTT_CLIENT: 执行 MQTT 重连...
I (42350) MQTT_CLIENT: MQTT 连接成功
```

---

## 八、常见问题

### Q1: 远程命令不生效？
**检查清单**:
1. WiFi 是否已连接？（查看 `MODE_REMOTE` 日志）
2. MQTT 是否已连接？（查看 `MQTT 连接成功` 日志）
3. 命令 JSON 格式是否正确？（查看 `收到远程命令` 日志）
4. 传感器是否正常？（故障时会进入 `MODE_SAFE_STOP`）

### Q2: 状态上报频率太高/太低？
修改 `main.h` 中的 `MQTT_PUBLISH_INTERVAL_SEC` 宏定义（默认 30 秒）。

### Q3: 如何测试远程命令？
使用 MQTT 客户端工具（如 MQTTX）发布消息到 `home/ventilation/command`：
```json
{"fan_state": "HIGH"}
```

---

## 九、相关文件索引

| 文件 | 功能 |
|------|------|
| `main/network/mqtt_wrapper.c` | MQTT 客户端实现 |
| `main/network/mqtt_wrapper.h` | MQTT 接口定义 |
| `main/algorithm/decision_engine.c` | 决策引擎（模式切换与风扇控制） |
| `main/main.c` | 主程序（任务调度与状态机） |
| `main/main.h` | 全局配置与数据结构定义 |
