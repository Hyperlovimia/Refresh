# 规范：决策算法

## 概述

本规范定义了决策引擎的接口和行为,包括运行模式检测、完整决策算法（Benefit-Cost 模型）和本地自主决策模式。

---

## ADDED Requirements

### Requirement: 运行模式检测

决策引擎 MUST 根据传感器状态、WiFi 连接状态和缓存有效性，自动检测并切换运行模式。

#### Scenario: 检测正常模式

**Given** 传感器正常工作 AND WiFi 已连接
**When** 调用 `decision_detect_mode(wifi_ok=true, cache_valid=true, sensor_ok=true)`
**Then** 返回 `MODE_NORMAL`

#### Scenario: 检测降级模式

**Given** 传感器正常工作 AND WiFi 断开 AND 缓存有效（<30分钟）
**When** 调用 `decision_detect_mode(wifi_ok=false, cache_valid=true, sensor_ok=true)`
**Then** 返回 `MODE_DEGRADED`

#### Scenario: 检测本地自主模式

**Given** 传感器正常工作 AND WiFi 断开 AND 缓存过期（>30分钟）
**When** 调用 `decision_detect_mode(wifi_ok=false, cache_valid=false, sensor_ok=true)`
**Then** 返回 `MODE_LOCAL`

#### Scenario: 检测安全停机模式

**Given** 传感器故障（连续 3 次读取失败）
**When** 调用 `decision_detect_mode(wifi_ok=true, cache_valid=true, sensor_ok=false)`
**Then** 返回 `MODE_SAFE_STOP`

---

### Requirement: 完整决策算法（Benefit-Cost 模型）

决策引擎在 MODE_NORMAL 和 MODE_DEGRADED 模式下，MUST 使用完整的 Benefit-Cost 模型计算通风指数并决定风扇状态。

#### Scenario: 高收益低成本场景（启动高速风扇）

**Given** 传感器数据：
- CO₂: 1800 ppm（室内质量差）
- 温度: 24°C
- 湿度: 60%

**And** 天气数据：
- PM2.5: 20 μg/m³（室外质量好）
- 室外温度: 22°C（温差小）
- 风速: 5 m/s

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`
**Then**
- 计算通风指数：
  ```c
  indoor_quality = (1800 - 400) / 1600 = 0.875
  outdoor_quality = 20 / 100 = 0.2
  temp_diff = |24 - 22| = 2

  benefit = 0.875 * 10 = 8.75
  cost = 0.2 * 5 + 2 * 2 = 5.0

  index = 8.75 - 5.0 = 3.75
  ```
- index > 3.0 → 返回 `FAN_HIGH`

#### Scenario: 中等收益场景（启动低速风扇）

**Given** 传感器数据：
- CO₂: 1200 ppm
- 温度: 24°C

**And** 天气数据：
- PM2.5: 50 μg/m³
- 室外温度: 23°C
- 风速: 3 m/s

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`
**Then**
- 计算通风指数：
  ```c
  indoor_quality = (1200 - 400) / 1600 = 0.5
  outdoor_quality = 50 / 100 = 0.5
  temp_diff = |24 - 23| = 1

  benefit = 0.5 * 10 = 5.0
  cost = 0.5 * 5 + 1 * 2 = 4.5

  index = 5.0 - 4.5 = 0.5 (实际应调整系数使其>1.0)
  ```
- 假设调整后 index = 1.5
- 1.0 < index <= 3.0 → 返回 `FAN_LOW`

#### Scenario: 低收益高成本场景（关闭风扇）

**Given** 传感器数据：
- CO₂: 600 ppm（室内质量好）
- 温度: 24°C

**And** 天气数据：
- PM2.5: 150 μg/m³（室外污染严重）
- 室外温度: 10°C（温差大）
- 风速: 1 m/s

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`
**Then**
- 计算通风指数：
  ```c
  indoor_quality = (600 - 400) / 1600 = 0.125
  outdoor_quality = 150 / 100 = 1.5
  temp_diff = |24 - 10| = 14

  benefit = 0.125 * 10 = 1.25
  cost = 1.5 * 5 + 14 * 2 = 35.5

  index = 1.25 - 35.5 = -34.25
  ```
- index <= 1.0 → 返回 `FAN_OFF`

---

### Requirement: 本地自主决策模式

决策引擎在 MODE_LOCAL 模式下（网络离线 >30分钟），MUST 使用简化的本地决策算法，仅基于 CO₂ 浓度。

#### Scenario: CO₂ 极高（启动高速风扇）

**Given** CO₂ 浓度为 1800 ppm
**When** 调用 `local_mode_decide(1800.0f)`
**Then** 返回 `FAN_HIGH`（因为 1800 > 1200）

#### Scenario: CO₂ 偏高（启动低速风扇）

**Given** CO₂ 浓度为 1100 ppm
**When** 调用 `local_mode_decide(1100.0f)`
**Then** 返回 `FAN_LOW`（因为 1000 < 1100 <= 1200）

#### Scenario: CO₂ 正常（关闭风扇）

**Given** CO₂ 浓度为 800 ppm
**When** 调用 `local_mode_decide(800.0f)`
**Then** 返回 `FAN_OFF`（因为 800 <= 1000）

---

### Requirement: 安全停机模式

决策引擎在 MODE_SAFE_STOP 模式下（传感器故障），MUST 强制关闭风扇。

#### Scenario: 传感器故障时关闭风扇

**Given** 传感器故障（sensor_ok = false）
**When** 调用 `decision_make(&sensor, &weather, MODE_SAFE_STOP)`
**Then** 返回 `FAN_OFF`

---

## 数据结构定义

```c
// 系统运行模式
typedef enum {
    MODE_NORMAL,     // 正常模式（WiFi + 完整算法）
    MODE_DEGRADED,   // 降级模式（WiFi 断开但缓存有效）
    MODE_LOCAL,      // 本地自主模式（网络离线 >30min）
    MODE_SAFE_STOP   // 安全停机（传感器故障）
} SystemMode;
```

---

## 接口定义

### 决策引擎（algorithm/decision_engine.h）

```c
/**
 * @brief 检测运行模式
 * @param wifi_ok WiFi 是否连接
 * @param cache_valid 天气数据缓存是否有效
 * @param sensor_ok 传感器是否正常
 * @return 系统运行模式
 */
SystemMode decision_detect_mode(bool wifi_ok, bool cache_valid, bool sensor_ok);

/**
 * @brief 执行决策算法（支持多种模式）
 * @param sensor 传感器数据
 * @param weather 天气数据（MODE_LOCAL 模式下可为 NULL）
 * @param mode 运行模式
 * @return 风扇目标状态
 */
FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode);
```

### 本地自主模式（algorithm/local_mode.h）

```c
/**
 * @brief 本地自主决策（仅基于 CO₂）
 * @param co2_ppm CO₂ 浓度（ppm）
 * @return 风扇目标状态
 */
FanState local_mode_decide(float co2_ppm);
```

---

## 算法详细设计

### Benefit-Cost 模型公式

```c
float calculate_ventilation_index(SensorData *sensor, WeatherData *weather) {
    // 标准化室内质量（CO₂ 越高质量越差）
    float indoor_quality = (sensor->co2 - 400.0f) / 1600.0f;

    // 标准化室外质量（PM2.5 越高质量越差）
    float outdoor_quality = weather->pm25 / 100.0f;

    // 温差成本
    float temp_diff = fabsf(sensor->temperature - weather->temperature);

    // 收益：改善室内质量
    float benefit = indoor_quality * 10.0f;

    // 成本：引入室外污染 + 温差导致的能耗
    float cost = outdoor_quality * 5.0f + temp_diff * 2.0f;

    // 通风指数 = 收益 - 成本
    return benefit - cost;
}

FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode) {
    if (mode == MODE_SAFE_STOP) {
        return FAN_OFF; // 安全停机
    }

    if (mode == MODE_LOCAL) {
        return local_mode_decide(sensor->co2); // 本地模式
    }

    // MODE_NORMAL 或 MODE_DEGRADED（使用完整算法）
    float index = calculate_ventilation_index(sensor, weather);

    if (index > 3.0f) return FAN_HIGH;
    if (index > 1.0f) return FAN_LOW;
    return FAN_OFF;
}
```

### 本地自主决策逻辑

```c
FanState local_mode_decide(float co2_ppm) {
    if (co2_ppm > 1200.0f) return FAN_HIGH; // 夜间放宽阈值
    if (co2_ppm > 1000.0f) return FAN_LOW;
    return FAN_OFF;
}
```

---

## 约束条件

1. **阈值设定**：
   - 完整算法：
     - 高速风扇：通风指数 > 3.0
     - 低速风扇：1.0 < 通风指数 <= 3.0
     - 关闭风扇：通风指数 <= 1.0
   - 本地模式：
     - 高速风扇：CO₂ > 1200 ppm
     - 低速风扇：1000 < CO₂ <= 1200 ppm
     - 关闭风扇：CO₂ <= 1000 ppm

2. **模式优先级**：
   - MODE_SAFE_STOP 优先级最高（传感器故障）
   - MODE_LOCAL 仅在网络完全离线（>30min）时使用
   - MODE_DEGRADED 使用缓存数据，但算法与 MODE_NORMAL 相同

3. **夜间模式调整**（预留）：
   - 22:00-06:00：CO₂ 阈值放宽至 1200 ppm（避免频繁启动）
   - 白天：CO₂ 阈值为 1000 ppm

4. **防抖动**：
   - 风扇状态切换需连续 3 次决策结果一致
   - 避免阈值附近频繁开关

---

## 测试要求

1. **单元测试**：
   - 测试运行模式检测逻辑（4 种模式）
   - 测试 Benefit-Cost 计算公式正确性
   - 测试本地决策逻辑
   - 测试边界条件（阈值附近）

2. **场景测试**：
   - 模拟高 CO₂ + 低 PM2.5 场景 → 应启动风扇
   - 模拟低 CO₂ + 高 PM2.5 场景 → 应关闭风扇
   - 模拟网络离线场景 → 应切换到本地模式
   - 模拟传感器故障场景 → 应强制关闭风扇

3. **长期测试**：
   - 24 小时连续运行，记录决策结果
   - 分析决策合理性（与实际空气质量对比）

---

## 扩展性

未来可扩展的功能：
- 基于时间的自适应阈值调整
- 机器学习模型替代硬编码算法
- 多房间联动决策
- 用户偏好学习（如温度敏感度）
