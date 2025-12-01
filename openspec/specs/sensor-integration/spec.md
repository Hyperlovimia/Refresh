# sensor-integration Specification

## Purpose
TBD - created by archiving change implement-air-quality-system. Update Purpose after archive.
## Requirements
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

