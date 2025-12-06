## ADDED Requirements

### Requirement: 室内污染物数据结构

系统 MUST 支持 4 类室内污染物的数据存储：CO2、颗粒物(PM)、挥发性有机物(VOC)、甲醛(HCHO)。

#### Scenario: 污染物数据结构定义

**Given** 系统需要存储多类污染物数据
**When** 定义 `IndoorPollutants` 结构体
**Then** 结构体包含以下字段：
```c
typedef struct {
    float co2;    ///< CO₂ 浓度（ppm），实际硬件采集
    float pm;     ///< 颗粒物浓度（μg/m³），预留接口
    float voc;    ///< 挥发性有机物（μg/m³），预留接口
    float hcho;   ///< 甲醛浓度（mg/m³），预留接口
} IndoorPollutants;
```

#### Scenario: SensorData 结构嵌入污染物数据

**Given** `SensorData` 原有 `float co2` 字段
**When** 扩展为多污染物支持
**Then** 修改为：
```c
typedef struct {
    IndoorPollutants pollutants;  ///< 室内污染物数据
    float temperature;            ///< 温度（摄氏度）
    float humidity;               ///< 相对湿度（%）
    time_t timestamp;             ///< 数据时间戳
    bool valid;                   ///< 数据有效标志
} SensorData;
```
- 原 `data.co2` 访问路径变更为 `data.pollutants.co2`

---

### Requirement: 污染物手动注入接口

系统 MUST 提供手动设置非 CO2 污染物数据的接口，用于测试和模拟。

#### Scenario: 手动设置颗粒物数据

**Given** 系统无颗粒物硬件传感器
**When** 调用 `sensor_manager_set_pollutant(POLLUTANT_PM, 35.0f)`
**Then** 下次 `sensor_manager_read_all()` 返回的 `data.pollutants.pm` 为 35.0f

#### Scenario: 手动设置 VOC 数据

**Given** 系统无 VOC 硬件传感器
**When** 调用 `sensor_manager_set_pollutant(POLLUTANT_VOC, 200.0f)`
**Then** 下次 `sensor_manager_read_all()` 返回的 `data.pollutants.voc` 为 200.0f

#### Scenario: 手动设置甲醛数据

**Given** 系统无甲醛硬件传感器
**When** 调用 `sensor_manager_set_pollutant(POLLUTANT_HCHO, 0.08f)`
**Then** 下次 `sensor_manager_read_all()` 返回的 `data.pollutants.hcho` 为 0.08f

---

## MODIFIED Requirements

### Requirement: 传感器管理器接口

传感器管理器 MUST 统一管理所有传感器，提供一次性读取所有数据和健康检查功能。

#### Scenario: 初始化传感器管理器

**Given** 所有传感器驱动尚未初始化
**When** 调用 `sensor_manager_init()`
**Then**
- 依次调用 `co2_sensor_init()` 和 `sht35_init()`
- 初始化手动注入的污染物数据为 0.0f
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
  data.pollutants.co2 = 850.0f;   // 实际硬件读取
  data.pollutants.pm = 0.0f;      // 手动注入值或默认 0
  data.pollutants.voc = 0.0f;     // 手动注入值或默认 0
  data.pollutants.hcho = 0.0f;    // 手动注入值或默认 0
  data.temperature = 24.5f;
  data.humidity = 58.0f;
  ```
- 函数返回 `ESP_OK`

#### Scenario: 读取传感器数据（CO₂ 传感器故障）

**Given** CO₂ 传感器连续 3 次读取失败
**When** 调用 `sensor_manager_read_all(&data)`
**Then**
- 检测到 CO₂ 传感器故障
- 使用上次有效值填充 `data.pollutants.co2`
- 函数返回 `ESP_FAIL`

#### Scenario: 检查传感器健康状态

**Given** 所有传感器正常工作
**When** 调用 `sensor_manager_is_healthy()`
**Then** 返回 `true`

**Given** CO₂ 传感器连续 3 次读取失败
**When** 调用 `sensor_manager_is_healthy()`
**Then** 返回 `false`
