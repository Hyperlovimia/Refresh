## MODIFIED Requirements

### Requirement: 完整决策算法（Benefit-Cost 模型）

决策引擎在 MODE_NORMAL 和 MODE_DEGRADED 模式下,MUST 使用完整的 Benefit-Cost 模型计算通风指数并决定风扇状态。

**修改说明**: 数据访问路径从 `sensor->co2` 变更为 `sensor->pollutants.co2`，算法逻辑不变。

#### 新增实现细节

**算法公式**:

```c
// 1. 归一化计算（访问路径变更）
indoor_quality = (sensor->pollutants.co2 - 400.0f) / 1600.0f;  // 室内质量指数 [0, 1]
outdoor_quality = pm25 / 100.0f;            // 室外质量指数 [0, ∞)
temp_diff = fabsf(indoor_temp - outdoor_temp);  // 温差(℃)

// 2. Benefit-Cost 计算
benefit = indoor_quality × 10.0f;           // 收益权重系数
cost = outdoor_quality × 5.0f + temp_diff × 2.0f;  // 成本权重系数
index = benefit - cost;                     // 通风指数

// 3. 决策逻辑
if (index > 3.0f) return FAN_HIGH;
else if (index > 1.0f) return FAN_LOW;
else return FAN_OFF;
```

**算法参数**:

| 参数 | 值 | 说明 |
|------|-----|------|
| CO2_BASELINE | 400.0f ppm | CO₂ 基准浓度(室外标准) |
| CO2_RANGE | 1600.0f ppm | CO₂ 归一化范围(400-2000 ppm) |
| PM25_RANGE | 100.0f μg/m³ | PM2.5 归一化范围 |
| BENEFIT_WEIGHT | 10.0f | CO₂ 收益权重系数 |
| PM25_COST_WEIGHT | 5.0f | PM2.5 成本权重系数 |
| TEMP_COST_WEIGHT | 2.0f | 温差成本权重系数 |
| INDEX_HIGH_THRESHOLD | 3.0f | 高速风扇阈值 |
| INDEX_LOW_THRESHOLD | 1.0f | 低速风扇阈值 |

**降级处理**:

- 当 `weather->valid == false` 时,MUST 自动降级为 `local_mode_decide(sensor->pollutants.co2)`
- 当 `sensor->valid == false` 时,MUST 返回 `FAN_OFF`

#### Scenario: 高收益低成本场景(启动高速风扇)

**Given** 传感器数据:
- CO₂: 1800 ppm(室内质量差)
- 温度: 24°C
- 湿度: 60%

**And** 天气数据:
- PM2.5: 20 μg/m³(室外质量好)
- 室外温度: 22°C(温差小)
- 风速: 5 m/s

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`

**Then**
- 从 `sensor.pollutants.co2` 获取 CO₂ 值
- 计算通风指数:
  ```c
  indoor_quality = (1800 - 400) / 1600 = 0.875
  outdoor_quality = 20 / 100 = 0.2
  temp_diff = |24 - 22| = 2

  benefit = 0.875 * 10 = 8.75
  cost = 0.2 * 5 + 2 * 2 = 5.0

  index = 8.75 - 5.0 = 3.75
  ```
- index > 3.0 → 返回 `FAN_HIGH`

#### Scenario: 低收益高成本场景(关闭风扇)

**Given** 传感器数据:
- CO₂: 600 ppm(室内质量好)
- 温度: 24°C

**And** 天气数据:
- PM2.5: 150 μg/m³(室外污染严重)
- 室外温度: 10°C(温差大)

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`

**Then**
- 从 `sensor.pollutants.co2` 获取 CO₂ 值
- index <= 1.0 → 返回 `FAN_OFF`

---

### Requirement: 本地自主决策模式

决策引擎在 MODE_LOCAL 模式下（网络离线 >30分钟），MUST 使用简化的本地决策算法，仅基于 CO₂ 浓度。

**修改说明**: 数据访问路径从 `sensor->co2` 变更为 `sensor->pollutants.co2`。

#### Scenario: CO₂ 极高（启动高速风扇）

**Given** CO₂ 浓度为 1800 ppm（从 `sensor.pollutants.co2` 获取）
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
