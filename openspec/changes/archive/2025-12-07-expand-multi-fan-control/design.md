# expand-multi-fan-control Design

## 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                      远程控制器 (服务器)                      │
└──────────────────────────┬──────────────────────────────────┘
                           │ MQTT
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                      mqtt_wrapper                           │
│  订阅: home/ventilation/command                             │
│  发布: home/ventilation/status                              │
│  命令格式: {"fan_0":"HIGH", "fan_1":"LOW", "fan_2":"OFF"}   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                    decision_engine                          │
│  MODE_REMOTE: 直接使用远程命令                               │
│  MODE_LOCAL:  3风扇同步（等待多传感器支持分区）               │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                     fan_control                             │
│  Fan0: GPIO26, LEDC_CHANNEL_0                               │
│  Fan1: GPIO27, LEDC_CHANNEL_1                               │
│  Fan2: GPIO33, LEDC_CHANNEL_2                               │
│  共用: LEDC_TIMER_0, 25kHz, 8-bit                           │
└─────────────────────────────────────────────────────────────┘
```

## 硬件资源分配

### GPIO 分配
| 风扇 | GPIO | 说明 |
|------|------|------|
| Fan0 | GPIO26 | 保持不变，原有风扇 |
| Fan1 | GPIO27 | 新增，与GPIO26相邻 |
| Fan2 | GPIO33 | 新增，ADC1通道可用于PWM |

### LEDC 资源
| 风扇 | 定时器 | 通道 | 频率 | 分辨率 |
|------|--------|------|------|--------|
| Fan0 | TIMER_0 | CHANNEL_0 | 25kHz | 8-bit |
| Fan1 | TIMER_0 | CHANNEL_1 | 25kHz | 8-bit |
| Fan2 | TIMER_0 | CHANNEL_2 | 25kHz | 8-bit |

**设计决策**：3个通道共用1个定时器，节省资源且保证频率一致。

## 数据结构设计

### main.h 新增定义

```c
// 风扇数量
#define FAN_COUNT 3

// 风扇ID枚举
typedef enum {
    FAN_ID_0 = 0,
    FAN_ID_1 = 1,
    FAN_ID_2 = 2
} FanId;

// 多风扇状态结构（可选，用于批量操作）
typedef struct {
    FanState states[FAN_COUNT];
} MultiFanState;
```

### fan_control 模块内部

```c
// GPIO 映射
static const gpio_num_t FAN_GPIO[FAN_COUNT] = {
    GPIO_NUM_26,  // Fan0
    GPIO_NUM_27,  // Fan1
    GPIO_NUM_33   // Fan2
};

// LEDC 通道映射
static const ledc_channel_t FAN_CHANNEL[FAN_COUNT] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2
};

// 状态数组
static FanState current_state[FAN_COUNT] = {FAN_OFF, FAN_OFF, FAN_OFF};
static uint8_t current_pwm[FAN_COUNT] = {0, 0, 0};
```

## 接口变更

### fan_control.h

```c
// 初始化（一次性初始化所有风扇）
esp_err_t fan_control_init(void);

// 单风扇控制
esp_err_t fan_control_set_state(FanId id, FanState state, bool is_night_mode);
esp_err_t fan_control_set_pwm(FanId id, uint8_t duty);
FanState fan_control_get_state(FanId id);
uint8_t fan_control_get_pwm(FanId id);

// 批量控制（便捷函数）
esp_err_t fan_control_set_all(FanState state, bool is_night_mode);
```

### mqtt_wrapper.h

```c
// 命令获取（输出3个风扇状态）
bool mqtt_get_remote_command(FanState cmd[FAN_COUNT]);

// 状态发布（输入3个风扇状态）
esp_err_t mqtt_publish_status(SensorData *sensor, const FanState fans[FAN_COUNT], SystemMode mode);
```

### decision_engine.h

```c
// 决策函数（输出3个风扇状态）
void decision_make(SensorData *sensor, const FanState remote_cmd[FAN_COUNT],
                   SystemMode mode, FanState out_fans[FAN_COUNT]);
```

## MQTT 协议设计

### 命令主题 `home/ventilation/command`

**请求格式**（服务器→设备）：
```json
{
  "fan_0": "HIGH",
  "fan_1": "LOW",
  "fan_2": "OFF"
}
```

**字段说明**：
- `fan_0`, `fan_1`, `fan_2`：各风扇状态，值为 "OFF" | "LOW" | "HIGH"
- 支持部分更新：只发送需要修改的风扇字段

### 状态主题 `home/ventilation/status`

**响应格式**（设备→服务器）：
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

## 本地模式行为

当网络离线（MODE_LOCAL）时：
1. 3个风扇根据**同一个CO2传感器**的读数统一决策
2. 所有风扇状态保持一致
3. 决策逻辑复用现有 `local_mode_decide()`

**后续扩展**：多传感器变更完成后，本地模式可实现分区控制。

## 兼容性考虑

### 向后兼容
- **不保留**旧API，直接替换
- 所有调用点需要同步修改

### 远程服务器兼容
- 服务器需要更新命令发送逻辑
- 状态解析需要适配新字段

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| GPIO33 可能有限制 | 中 | 硬件测试验证，备选GPIO12 |
| 服务器未及时更新 | 低 | 协议文档先行，通知服务器开发者 |
| LEDC通道不足 | 低 | ESP32-S3有8通道，3通道绰绰有余 |
