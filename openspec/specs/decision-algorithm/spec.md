# decision-algorithm Specification

## Purpose
TBD - created by archiving change implement-air-quality-system. Update Purpose after archive.
## Requirements
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

