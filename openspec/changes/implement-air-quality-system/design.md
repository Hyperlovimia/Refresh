# 设计文档：智能室内空气质量改善系统

## 1. 系统架构

### 1.1 整体架构

系统采用分层架构设计，从下到上分为：硬件抽象层、业务逻辑层、应用层。

```
┌─────────────────────────────────────────────────────────────┐
│                      应用层 (Application)                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  main.c      │  │ OLED Display │  │ MQTT Client  │      │
│  │  系统编排     │  │  用户界面     │  │  远程监控     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                  业务逻辑层 (Business Logic)                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Decision     │  │ Sensor       │  │ Weather API  │      │
│  │ Engine       │  │ Manager      │  │ Client       │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                  硬件抽象层 (Hardware Abstraction)           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ CO₂ Sensor   │  │ SHT35        │  │ Fan Control  │      │
│  │ (UART)       │  │ (I2C)        │  │ (PWM)        │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                   ESP-IDF HAL & FreeRTOS                     │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 模块依赖关系

```
main.c
├── sensors/sensor_manager
│   ├── sensors/co2_sensor
│   └── sensors/sht35
├── actuators/fan_control
├── algorithm/decision_engine
│   └── algorithm/local_mode
├── network/wifi_manager
├── network/weather_api
├── network/mqtt_client
└── ui/oled_display
```

**依赖原则**：
- 上层可依赖下层，下层不可依赖上层
- 同层模块之间通过数据结构传递信息，避免直接调用
- 所有硬件抽象层模块独立，不相互依赖

---

## 2. 数据流设计

### 2.1 主数据流

```
传感器读取 (1Hz)
  ↓
[SensorData 结构体]
  ↓
┌────────────────────┐
│ 共享数据缓冲区      │ ← 互斥锁保护
│ - sensor_data      │
│ - weather_data     │
│ - fan_state        │
└────────────────────┘
  ↓
决策任务 (1Hz)
  ↓
[FanState 枚举]
  ↓
风扇控制
```

### 2.2 网络数据流

```
天气 API 请求 (10分钟周期)
  ↓
[WeatherData 结构体] → 缓存 (30分钟有效期)
  ↓
决策任务读取
```

```
MQTT 上报 (30秒周期或状态变化时)
  ↓
发布到 home/ventilation/status
  {
    "co2": 850,
    "temp": 24.5,
    "humi": 58,
    "fan_state": "LOW",
    "mode": "NORMAL"
  }
```

### 2.3 状态机数据流

```
系统状态 (SystemState)
  ↓
OLED 显示任务 ← 读取状态并显示倒计时/提示
  ↓
用户可见的状态信息
```

---

## 3. 并发模型

### 3.1 任务设计

采用 FreeRTOS 多任务模型，每个任务独立运行，通过共享数据结构和互斥锁通信。

| 任务名称 | 优先级 | 栈大小 | 周期 | 职责 |
|---------|-------|-------|-----|-----|
| sensor_task | 3 | 4KB | 1Hz | 读取传感器数据 |
| decision_task | 3 | 4KB | 1Hz | 执行决策算法 |
| network_task | 2 | 8KB | 10分钟 | 天气 API + MQTT |
| display_task | 2 | 4KB | 0.5Hz | 更新 OLED 显示 |
| app_main (主任务) | 4 | 8KB | 1Hz | 状态机管理 |

**优先级说明**：
- 优先级 4（最高）：主任务，负责系统健康监控和安全保护
- 优先级 3（中）：传感器和决策任务，核心业务逻辑
- 优先级 2（低）：网络和显示任务，非关键路径

### 3.2 同步机制

**互斥锁 (Mutex)**：
- `data_mutex`：保护共享数据缓冲区（sensor_data、weather_data、fan_state）
- `i2c_mutex`：保护 I2C 总线访问（SHT35 和 OLED 共享 I2C）

**事件组 (Event Group)**：
- `system_events`：
  - BIT0：WiFi 连接成功
  - BIT1：传感器预热完成
  - BIT2：传感器稳定完成
  - BIT3：传感器故障

**队列 (Queue)**：
- `alert_queue`：传递告警消息到显示任务（容量 5）

### 3.3 任务通信示例

```c
// 传感器任务写入数据
void sensor_task(void *pvParameters) {
    SensorData data;
    while (1) {
        sensor_manager_read_all(&data);

        xSemaphoreTake(data_mutex, portMAX_DELAY);
        memcpy(&shared_sensor_data, &data, sizeof(SensorData));
        xSemaphoreGive(data_mutex);

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz
    }
}

// 决策任务读取数据
void decision_task(void *pvParameters) {
    SensorData sensor;
    WeatherData weather;
    FanState new_state;

    while (1) {
        xSemaphoreTake(data_mutex, portMAX_DELAY);
        memcpy(&sensor, &shared_sensor_data, sizeof(SensorData));
        memcpy(&weather, &shared_weather_data, sizeof(WeatherData));
        xSemaphoreGive(data_mutex);

        new_state = decision_make(&sensor, &weather, current_mode);
        fan_control_set_state(new_state);

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz
    }
}
```

---

## 4. 状态机设计

### 4.1 系统状态定义

```c
typedef enum {
    STATE_INIT,         // 初始化状态（模块启动）
    STATE_PREHEATING,   // 预热状态（60秒）
    STATE_STABILIZING,  // 稳定状态（240秒）
    STATE_RUNNING,      // 正常运行状态
    STATE_ERROR         // 错误状态（传感器故障）
} SystemState;
```

### 4.2 状态转换图

```
           ┌─────────────┐
           │    INIT     │ 启动
           └──────┬──────┘
                  │ 所有模块初始化成功
                  ↓
           ┌─────────────┐
           │ PREHEATING  │ 60秒计时器
           └──────┬──────┘
                  │ 计时器到期
                  ↓
           ┌─────────────┐
           │STABILIZING  │ 240秒计时器
           └──────┬──────┘
                  │ 计时器到期
                  ↓
           ┌─────────────┐
      ┌───→│  RUNNING    │
      │    └──────┬──────┘
      │           │ 传感器故障
      │           ↓
      │    ┌─────────────┐
      │    │    ERROR    │ 安全停机（关闭风扇）
      │    └──────┬──────┘
      │           │ 传感器恢复
      └───────────┘
```

### 4.3 状态处理逻辑

**INIT 状态**：
- 调用所有模块的 `init()` 函数
- 检查返回值，任何失败 → 进入 ERROR 状态
- 全部成功 → 设置预热计时器，进入 PREHEATING 状态

**PREHEATING 状态**：
- 显示倒计时（60秒）
- 读取 CO₂ 数据但标记为"预热中"
- 风扇保持关闭
- 计时器到期 → 进入 STABILIZING 状态

**STABILIZING 状态**：
- 显示倒计时（240秒）
- CO₂ 数据标记为"稳定中"
- 允许决策但降低阈值（如 CO₂ > 1200ppm 才启动风扇）
- 计时器到期 → 进入 RUNNING 状态

**RUNNING 状态**：
- 正常执行决策算法
- 监控传感器健康状态
- 传感器故障 → 进入 ERROR 状态

**ERROR 状态**：
- 关闭风扇（`fan_control_set_state(FAN_OFF)`）
- 显示告警信息
- 定期检查传感器状态，恢复后 → 返回 INIT 状态重新初始化

---

## 5. 运行模式设计

### 5.1 运行模式定义

```c
typedef enum {
    MODE_NORMAL,    // 正常模式（WiFi + 天气数据 + 完整算法）
    MODE_DEGRADED,  // 降级模式（WiFi 断开但缓存有效）
    MODE_LOCAL,     // 本地模式（网络离线 >30min，仅 CO₂ 决策）
    MODE_SAFE_STOP  // 安全停机（传感器故障）
} SystemMode;
```

### 5.2 模式检测逻辑

```c
SystemMode decision_detect_mode(bool wifi_ok, bool cache_valid, bool sensor_ok) {
    if (!sensor_ok) {
        return MODE_SAFE_STOP; // 传感器故障优先级最高
    }

    if (wifi_ok) {
        return MODE_NORMAL; // WiFi 正常，使用完整算法
    }

    if (cache_valid) {
        return MODE_DEGRADED; // WiFi 断开但缓存有效（<30min）
    }

    return MODE_LOCAL; // 完全离线，本地自主决策
}
```

### 5.3 决策算法选择

**MODE_NORMAL / MODE_DEGRADED**：使用完整决策引擎
```c
float calculate_ventilation_index(SensorData *sensor, WeatherData *weather) {
    float indoor_quality = (sensor->co2 - 400) / 1600.0f;  // 标准化
    float outdoor_quality = weather->pm25 / 100.0f;
    float temp_diff = fabsf(sensor->temp - weather->temp);

    // Benefit-Cost 模型
    float benefit = indoor_quality * 10.0f;  // 改善室内质量的收益
    float cost = outdoor_quality * 5.0f + temp_diff * 2.0f; // 引入室外污染和温差的成本

    return benefit - cost;
}

FanState decision_make(...) {
    float index = calculate_ventilation_index(sensor, weather);

    if (index > 3.0f) return FAN_HIGH;
    if (index > 1.0f) return FAN_LOW;
    return FAN_OFF;
}
```

**MODE_LOCAL**：使用简化本地决策
```c
FanState local_mode_decide(float co2_ppm) {
    if (co2_ppm > 1200.0f) return FAN_HIGH; // 夜间放宽阈值
    if (co2_ppm > 1000.0f) return FAN_LOW;
    return FAN_OFF;
}
```

**MODE_SAFE_STOP**：强制关闭风扇
```c
return FAN_OFF;
```

---

## 6. 硬件接口设计

### 6.1 CO₂ 传感器 (UART)

**引脚连接**：
- ESP32 GPIO17 (TX) → 传感器 Pin4 (RX)
- ESP32 GPIO16 (RX) ← 传感器 Pin3 (TX)
- 5V → 传感器 Pin1 (VCC)
- GND → 传感器 Pin2 (GND)
- Pin6 (HD) 悬空（禁止自动校准）

**协议**：主动上报模式（ASCII）
- 格式：`"  xxxx ppm\r\n"`（2个空格 + 4位数字 + 空格 + "ppm" + 回车换行）
- 周期：1秒自动推送
- 解析逻辑：
  1. 搜索 "ppm" 关键字
  2. 向前提取数字部分
  3. 使用 `atoi()` 转换
  4. 范围检查（300-5000ppm）

**驱动接口**：
```c
esp_err_t co2_sensor_init(void) {
    // 配置 UART：9600, 8N1, RX缓冲1024字节
}

float co2_sensor_read_ppm(void) {
    // 从 UART 缓冲区读取数据，解析 ASCII 格式
    // 返回 CO₂ 浓度（ppm），失败返回 -1.0f
}
```

### 6.2 温湿度传感器 (I2C)

**引脚连接**：
- ESP32 GPIO22 (SCL) → SHT35 SCL
- ESP32 GPIO21 (SDA) → SHT35 SDA
- 3.3V → SHT35 VCC
- GND → SHT35 GND

**地址**：0x44（7位地址）

**协议**：I2C 高精度测量模式
- 发送命令：0x2C 0x06（高重复性测量）
- 等待 15ms
- 读取 6 字节（温度高、温度低、CRC、湿度高、湿度低、CRC）
- 计算公式：
  - 温度 = -45 + 175 * (原始值 / 65535)
  - 湿度 = 100 * (原始值 / 65535)

**驱动接口**：
```c
esp_err_t sht35_init(void) {
    // 配置 I2C：100kHz, GPIO21/22
}

esp_err_t sht35_read(float *temp, float *humi) {
    // 发送测量命令，延时，读取数据，计算温湿度
}
```

### 6.3 OLED 显示屏 (I2C)

**引脚连接**：
- 与 SHT35 共享 I2C 总线（GPIO21/22）
- 地址：0x3C（7位地址）

**协议**：SSD1306 命令/数据协议
- 分辨率：128×64 单色
- 使用 u8g2 库简化开发

**驱动接口**：
```c
esp_err_t oled_display_init(void) {
    // 初始化 u8g2，配置 I2C
}

void oled_display_main_page(...) {
    // 使用 u8g2 绘制主页面（数据、趋势图、状态栏）
}
```

**I2C 总线保护**：
```c
xSemaphoreTake(i2c_mutex, portMAX_DELAY);
// 执行 I2C 操作（SHT35 或 OLED）
xSemaphoreGive(i2c_mutex);
```

### 6.4 风扇控制 (PWM)

**引脚连接**：
- ESP32 GPIO（待选） → MOSFET 栅极 → 风扇正极
- 风扇负极 → GND

**PWM 配置**：
- 频率：25kHz（超声频率，避免噪音）
- 分辨率：8位（0-255）
- 占空比：
  - OFF: 0%
  - LOW: 50%（占空比 128/255）
  - HIGH: 100%（占空比 255/255）

**驱动接口**：
```c
esp_err_t fan_control_init(void) {
    // 配置 LEDC PWM：25kHz, 8位分辨率
}

esp_err_t fan_control_set_state(FanState state) {
    // 根据 state 设置占空比
}
```

---

## 7. 网络服务设计

### 7.1 WiFi 管理

**配网方式**：SmartConfig（ESP-Touch）
- 用户通过手机 APP 输入 WiFi 密码
- ESP32 进入混杂模式监听，提取密码
- 保存到 NVS，下次自动连接

**重连策略**：
- 连接失败：每 5 秒重试，最多 3 次
- 连接成功后断开：每 10 秒重试，无限次

**接口**：
```c
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_provisioning(void); // 首次配网
bool wifi_manager_is_connected(void);
```

### 7.2 天气 API 客户端

**服务商**：和风天气
- API URL：`https://api.qweather.com/v7/air/now`
- 参数：`location=城市代码&key=API_KEY`
- 响应格式：JSON

**数据提取**：
```json
{
  "now": {
    "pm2p5": "35",
    "temp": "22",
    "windSpeed": "12"
  }
}
```

**缓存策略**：
- 缓存有效期：30分钟
- 过期后优先使用实时数据，失败则继续使用过期缓存
- 缓存结构：
  ```c
  typedef struct {
      float pm25;
      float temperature;
      float wind_speed;
      time_t timestamp;
      bool valid;
  } WeatherCache;
  ```

**接口**：
```c
esp_err_t weather_api_fetch(WeatherData *data); // 获取实时数据
void weather_api_get_cached(WeatherData *data); // 获取缓存
bool weather_api_is_cache_stale(void);          // 检查缓存是否过期
```

### 7.3 MQTT 客户端

**Broker**：EMQX Cloud
- 连接 URL：`mqtts://xxx.emqxsl.cn:8883`
- 认证：用户名/密码
- TLS：启用（esp_crt_bundle_attach）

**主题设计**：
- 发布：`home/ventilation/status`（设备状态）
- 发布：`home/ventilation/alert`（告警消息）
- 订阅：`home/ventilation/command`（远程控制，预留）

**QoS 等级**：
- 状态数据：QoS 0（最多一次）
- 告警数据：QoS 1（至少一次）

**发布频率**：
- 状态数据：30秒周期或状态变化时
- 告警数据：事件触发（如 CO₂ > 1500ppm）

**接口**：
```c
esp_err_t mqtt_client_init(void);
esp_err_t mqtt_publish_status(SensorData *sensor, FanState fan);
esp_err_t mqtt_publish_alert(const char *message);
```

---

## 8. 配置管理

### 8.1 menuconfig 配置项

使用 ESP-IDF 的 Kconfig 系统，在 `menuconfig` 中提供配置：

```
Air Quality System Configuration
├── WiFi Configuration
│   ├── Default SSID (留空，支持配网)
│   └── Default Password (留空)
├── Weather API
│   ├── API Key (留空)
│   └── Location Code (留空)
├── MQTT Configuration
│   ├── Broker URL (留空)
│   ├── Username (留空)
│   └── Password (留空)
└── Sensor Configuration
    ├── CO₂ Preheating Time (默认 60秒)
    ├── CO₂ Stabilizing Time (默认 240秒)
    └── Decision Threshold (CO₂ 高速阈值，默认 1200ppm)
```

### 8.2 NVS 存储

使用 NVS（Non-Volatile Storage）保存运行时配置：
- WiFi SSID/密码（配网后保存）
- 上次校准时间（用于提示）
- 系统运行统计（可选）

---

## 9. 错误处理和降级策略

### 9.1 传感器故障处理

**检测**：
- 连续 3 次读取失败 → 判定为故障
- 数据超出合理范围（CO₂ <300 或 >5000）→ 标记异常

**处理**：
- 进入 ERROR 状态
- 关闭风扇（安全停机）
- OLED 显示"传感器故障"
- 每 10 秒重试一次
- 恢复后重新初始化

### 9.2 网络故障处理

**WiFi 断开**：
- 缓存有效（<30min）→ MODE_DEGRADED（使用缓存数据）
- 缓存过期（>30min）→ MODE_LOCAL（仅 CO₂ 决策）
- OLED 显示"⚠️ 网络离线"

**天气 API 请求失败**：
- 使用上次缓存数据
- 10 分钟后重试

**MQTT 断开**：
- 不影响核心功能
- 自动重连（每 30 秒）

### 9.3 I2C 总线冲突

**问题**：SHT35 和 OLED 共享 I2C 总线
**解决**：使用互斥锁
```c
xSemaphoreTake(i2c_mutex, portMAX_DELAY);
// 执行 I2C 操作
xSemaphoreGive(i2c_mutex);
```

---

## 10. 性能和资源估算

### 10.1 内存占用

**静态内存**：
- 代码段：约 150KB
- 数据段：约 20KB
- FreeRTOS 栈：
  - sensor_task: 4KB
  - decision_task: 4KB
  - network_task: 8KB
  - display_task: 4KB
  - main_task: 8KB
  - **总计**：28KB

**动态内存**：
- UART RX 缓冲区：1KB
- HTTPS 缓冲区：2KB
- MQTT 缓冲区：2KB
- u8g2 缓冲区：1KB
- **总计**：约 6KB

**总估算**：约 200KB（ESP32-S3 有 512KB SRAM，充足）

### 10.2 CPU 占用

- 传感器读取：<5%（1Hz，简单 UART/I2C 读取）
- 决策算法：<10%（1Hz，浮点运算）
- 网络通信：<15%（10分钟周期，HTTPS + JSON 解析）
- OLED 显示：<10%（0.5Hz，u8g2 绘制）

**总估算**：峰值 <40%，平均 <20%

### 10.3 功耗估算

- ESP32-S3（WiFi 开启）：约 100mA
- CO₂ 传感器：约 60mA
- SHT35：约 1mA
- OLED：约 20mA
- 风扇（最大功率）：约 2000mA（由外部供电）

**总计**：约 180mA @ 5V（ESP32 相关），风扇独立供电

---

## 11. 测试策略

### 11.1 单元测试

**覆盖范围**：
- 状态机转换逻辑（INIT → PREHEATING → STABILIZING → RUNNING → ERROR）
- 决策算法（完整算法、本地模式）
- 数据解析（CO₂ ASCII 解析、JSON 解析）
- 缓存管理（过期检测、降级策略）

**框架**：ESP-IDF Unity 测试框架

### 11.2 集成测试

**硬件在环测试**：
- CO₂ 传感器读取（人为呼气提高浓度）
- SHT35 温湿度读取
- 风扇 PWM 控制（万用表测量占空比）
- OLED 显示正确性

### 11.3 压力测试

- 长期运行稳定性（7天×24小时）
- 网络频繁断开/重连
- 传感器故障注入（拔插传感器）

---

## 12. 扩展性设计

### 12.1 硬件扩展

**预留接口**：
- 物理按钮（手动校准、菜单导航）
- 蜂鸣器（告警提示）
- SD 卡（日志记录）

**模块化设计**：
- 传感器驱动独立，更换传感器只需替换驱动文件
- 决策算法独立，可替换为 AI 模型

### 12.2 功能扩展

- 远程控制（MQTT 订阅 command 主题）
- 多房间联动（多设备 MQTT 协作）
- 历史数据分析（SD 卡日志 + 云端分析）
- 语音提示（集成 TTS 模块）

---

## 13. 安全性考虑

### 13.1 网络安全

- **TLS/HTTPS**：天气 API 和 MQTT 均使用加密通信
- **证书验证**：使用 `esp_crt_bundle_attach` 验证服务器证书
- **密钥管理**：API Key 和 MQTT 密码通过 menuconfig 配置，不硬编码

### 13.2 数据安全

- **NVS 加密**：可选启用 NVS 加密（存储 WiFi 密码）
- **输入验证**：对传感器数据进行范围检查，防止异常值干扰决策

### 13.3 系统安全

- **看门狗**：启用任务看门狗（Task Watchdog），防止任务死锁
- **安全停机**：传感器故障时强制关闭风扇，避免误操作

---

## 14. 关键技术决策总结

| 决策点 | 方案 | 理由 |
|-------|-----|------|
| 主控芯片 | ESP32-S3 | 双核、WiFi+BLE、足够的 GPIO 和内存 |
| CO₂ 传感器协议 | 主动上报模式 | 简单、自动推送、无需查询命令 |
| I2C 总线共享 | SHT35 + OLED 共享 | 节省 GPIO，使用互斥锁避免冲突 |
| 任务调度 | FreeRTOS 多任务 | 并发处理传感器、决策、网络、显示 |
| 决策算法 | Benefit-Cost 模型 + 本地模式 | 完整算法考虑多因素，本地模式保证离线可用 |
| 网络降级 | 缓存 + 本地决策 | 提高可靠性，避免网络依赖 |
| 配网方式 | SmartConfig | 用户友好，无需硬编码 WiFi 密码 |
| 显示库 | u8g2 | 成熟、支持 SSD1306、易于使用 |
| 安全通信 | TLS/HTTPS | 符合最佳实践，防止中间人攻击 |
| 测试策略 | 单元测试 + 硬件验证 | 算法逻辑自动化，硬件交互人工验证 |

---

## 15. 风险缓解措施

| 风险 | 影响 | 缓解措施 | 状态 |
|-----|------|---------|------|
| CO₂ 传感器预热时间长 | 启动后 5 分钟内无法正常决策 | 状态机管理预热/稳定阶段，OLED 显示倒计时 | 已设计 |
| 网络不稳定 | 无法获取天气数据 | 缓存机制（30min）+ 本地自主决策模式 | 已设计 |
| I2C 总线冲突 | SHT35 和 OLED 读写失败 | 互斥锁保护 I2C 访问 | 已设计 |
| 传感器故障 | 系统失效 | 检测故障 → 安全停机 → 定期重试 | 已设计 |
| 硬件验证困难 | 无法自动化测试 | 算法单元测试 + 人工硬件验证 | 已规划 |
| 模块接口变更 | 集成困难 | 第一阶段确定完整接口，后续严格遵循 | 已规划 |

---

## 16. 后续优化方向

1. **AI 决策模型**：训练 ML 模型替代硬编码算法
2. **能耗优化**：动态调整传感器采样频率和 WiFi 连接时长
3. **多设备协同**：通过 MQTT 实现多房间联动
4. **云端分析**：上传历史数据到云端，生成空气质量报告
5. **OTA 升级**：支持固件远程更新

---

## 附录：参考资料

- ESP-IDF 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/
- FreeRTOS 文档：https://www.freertos.org/Documentation/
- u8g2 库文档：https://github.com/olikraus/u8g2
- 和风天气 API：https://dev.qweather.com/docs/api/
- EMQX MQTT 文档：https://www.emqx.io/docs/
- SHT35 数据手册：https://www.sensirion.com/sht3x
- SSD1306 数据手册：https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
