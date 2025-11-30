# 规范：传感器集成

## 概述

本规范定义了传感器子系统的接口和行为，包括 CO₂ 传感器（JX-CO2-102-5K，UART）、温湿度传感器（SHT35，I2C）以及传感器管理器。

---

## ADDED Requirements

### Requirement: CO₂ 传感器驱动接口

传感器驱动 MUST 提供初始化、数据读取、状态检查和校准功能。

#### Scenario: 初始化 CO₂ 传感器

**Given** ESP32-S3 GPIO16/17 已配置为 UART RX/TX
**When** 调用 `co2_sensor_init()`
**Then**
- UART 配置为 9600 波特率、8位数据、无校验、1停止位
- RX 缓冲区分配 1024 字节
- 函数返回 `ESP_OK`

#### Scenario: 读取 CO₂ 浓度（正常情况）

**Given** 传感器已初始化并预热完成
**When** 调用 `co2_sensor_read_ppm()`
**Then**
- 从 UART 缓冲区读取数据（格式：`"  xxxx ppm\r\n"`）
- 解析数字部分（如 `1235`）
- 返回浮点数值（如 `1235.0f`）

#### Scenario: 读取 CO₂ 浓度（数据异常）

**Given** 传感器返回超出范围的数据（如 `9999 ppm`）
**When** 调用 `co2_sensor_read_ppm()`
**Then**
- 检测到数值 > 5000 或 < 300
- 返回 `-1.0f` 表示读取失败

#### Scenario: 检查传感器就绪状态

**Given** 传感器刚上电（预热时间 <60秒）
**When** 调用 `co2_sensor_is_ready()`
**Then** 返回 `false`

**Given** 传感器预热完成（>300秒）
**When** 调用 `co2_sensor_is_ready()`
**Then** 返回 `true`

#### Scenario: 手动校准（预留功能）

**Given** 传感器位于室外环境（CO₂ ≈ 400ppm）
**When** 调用 `co2_sensor_calibrate()`
**Then**
- 发送校准命令（`FF 01 05 07 00 00 00 00 F4`）
- 等待响应（`FF 01 03 07 01 00 00 00 F5`）
- 函数返回 `ESP_OK`

---

### Requirement: 温湿度传感器驱动接口

温湿度传感器驱动 MUST 提供初始化和数据读取功能。

#### Scenario: 初始化 SHT35 传感器

**Given** ESP32-S3 GPIO21/22 已配置为 I2C SDA/SCL
**When** 调用 `sht35_init()`
**Then**
- I2C 配置为 100kHz 标准模式
- I2C 地址设置为 `0x44`
- 函数返回 `ESP_OK`

#### Scenario: 读取温湿度（正常情况）

**Given** SHT35 已初始化
**When** 调用 `sht35_read(&temp, &humi)`
**Then**
- 发送高精度测量命令（`0x2C 0x06`）
- 延时 15ms 等待测量完成
- 读取 6 字节数据（温度高、温度低、CRC、湿度高、湿度低、CRC）
- 计算温度：`temp = -45 + 175 * (raw / 65535)`
- 计算湿度：`humi = 100 * (raw / 65535)`
- 函数返回 `ESP_OK`

#### Scenario: 读取温湿度（I2C 通信失败）

**Given** I2C 总线被占用或传感器未连接
**When** 调用 `sht35_read(&temp, &humi)`
**Then**
- 检测到 I2C 读取失败
- 函数返回 `ESP_FAIL`
- `temp` 和 `humi` 保持原值不变

---

### Requirement: 传感器管理器接口

传感器管理器 MUST 统一管理所有传感器，提供一次性读取所有数据和健康检查功能。

#### Scenario: 初始化传感器管理器

**Given** 所有传感器驱动尚未初始化
**When** 调用 `sensor_manager_init()`
**Then**
- 依次调用 `co2_sensor_init()` 和 `sht35_init()`
- 如果任一初始化失败，返回 `ESP_FAIL`
- 所有初始化成功，返回 `ESP_OK`

#### Scenario: 读取所有传感器数据（正常情况）

**Given** 传感器管理器已初始化
**When** 调用 `sensor_manager_read_all(&data)`
**Then**
- 调用 `co2_sensor_read_ppm()` 读取 CO₂ 浓度
- 调用 `sht35_read(&temp, &humi)` 读取温湿度
- 填充 `SensorData` 结构体：
  ```c
  data.co2 = 850.0f;
  data.temperature = 24.5f;
  data.humidity = 58.0f;
  ```
- 函数返回 `ESP_OK`

#### Scenario: 读取传感器数据（CO₂ 传感器故障）

**Given** CO₂ 传感器连续 3 次读取失败
**When** 调用 `sensor_manager_read_all(&data)`
**Then**
- 检测到 CO₂ 传感器故障
- 使用上次有效值填充 `data.co2`
- 函数返回 `ESP_FAIL`

#### Scenario: 检查传感器健康状态

**Given** 所有传感器正常工作
**When** 调用 `sensor_manager_is_healthy()`
**Then** 返回 `true`

**Given** CO₂ 传感器连续 3 次读取失败
**When** 调用 `sensor_manager_is_healthy()`
**Then** 返回 `false`

---

## 数据结构定义

```c
// 传感器数据结构
typedef struct {
    float co2;          // CO₂ 浓度（ppm）
    float temperature;  // 温度（°C）
    float humidity;     // 相对湿度（%）
} SensorData;
```

---

## 接口定义

### CO₂ 传感器驱动（sensors/co2_sensor.h）

```c
/**
 * @brief 初始化 CO₂ 传感器
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t co2_sensor_init(void);

/**
 * @brief 读取 CO₂ 浓度
 * @return CO₂ 浓度（ppm），失败返回 -1.0f
 */
float co2_sensor_read_ppm(void);

/**
 * @brief 检查传感器是否就绪（预热完成）
 * @return true 就绪, false 预热中
 */
bool co2_sensor_is_ready(void);

/**
 * @brief 手动校准传感器（需在室外 400ppm 环境下执行）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t co2_sensor_calibrate(void);
```

### 温湿度传感器驱动（sensors/sht35.h）

```c
/**
 * @brief 初始化 SHT35 传感器
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t sht35_init(void);

/**
 * @brief 读取温湿度
 * @param temp 输出参数，温度（°C）
 * @param humi 输出参数，相对湿度（%）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t sht35_read(float *temp, float *humi);
```

### 传感器管理器（sensors/sensor_manager.h）

```c
/**
 * @brief 初始化传感器管理器（内部调用所有传感器的 init 函数）
 * @return ESP_OK 成功, ESP_FAIL 任一传感器初始化失败
 */
esp_err_t sensor_manager_init(void);

/**
 * @brief 读取所有传感器数据
 * @param data 输出参数，传感器数据结构
 * @return ESP_OK 成功, ESP_FAIL 读取失败
 */
esp_err_t sensor_manager_read_all(SensorData *data);

/**
 * @brief 检查传感器健康状态
 * @return true 健康, false 故障
 */
bool sensor_manager_is_healthy(void);
```

---

## 约束条件

1. **CO₂ 传感器预热**：
   - 上电后 60 秒内数据不可信
   - 60-300 秒数据逐渐稳定
   - >300 秒数据完全稳定

2. **I2C 总线共享**：
   - SHT35 与 OLED 共享 I2C 总线
   - 必须使用互斥锁保护 I2C 访问

3. **错误恢复**：
   - 单次读取失败：使用上次有效值
   - 连续 3 次失败：标记传感器故障
   - 传感器故障后：每 10 秒重试一次

4. **数据范围**：
   - CO₂：300-5000 ppm
   - 温度：-40 - 125 °C
   - 湿度：0 - 100 %

---

## 测试要求

1. **单元测试**：
   - CO₂ ASCII 数据解析正确性
   - SHT35 原始数据转换公式正确性
   - 错误处理逻辑（超出范围、连续失败）

2. **集成测试**：
   - 人为呼气提高 CO₂ 浓度，验证读取正确
   - SHT35 读取室温室湿，验证数值合理
   - I2C 总线并发访问（SHT35 + OLED），无冲突

3. **长期测试**：
   - 连续运行 24 小时，无内存泄漏
   - 传感器数据稳定性（标准差 <5%）
