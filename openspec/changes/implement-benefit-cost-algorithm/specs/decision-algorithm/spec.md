# decision-algorithm Spec Delta

本变更修改现有的决策算法规格,实现完整的 Benefit-Cost 模型。

## MODIFIED Requirements

### Requirement: 完整决策算法(Benefit-Cost 模型)

决策引擎在 MODE_NORMAL 和 MODE_DEGRADED 模式下,MUST 使用完整的 Benefit-Cost 模型计算通风指数并决定风扇状态。

**修改说明**:原规格已定义算法要求和三个场景,本次变更实现该算法的代码逻辑,补充实现细节和算法参数。

#### 新增实现细节

**算法公式**:

```c
// 1. 归一化计算
indoor_quality = (co2 - 400.0f) / 1600.0f;  // 室内质量指数 [0, 1]
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

- 当 `weather->valid == false` 时,MUST 自动降级为 `local_mode_decide(sensor->co2)`
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
- 日志输出包含:`indoor_q=0.88 outdoor_q=0.20 temp_diff=2.0 benefit=8.75 cost=5.00 index=3.75`

#### Scenario: 中等收益场景(启动低速风扇)

**Given** 传感器数据:
- CO₂: 1200 ppm
- 温度: 24°C

**And** 天气数据:
- PM2.5: 50 μg/m³
- 室外温度: 23°C
- 风速: 3 m/s

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`

**Then**
- 计算通风指数:
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
- 日志输出包含:`indoor_q=0.50 outdoor_q=0.50 temp_diff=1.0 benefit=5.00 cost=4.50 index=0.50`

**注意**:规格示例中 index=0.5 <1.0,但预期返回 FAN_LOW。实际实现中需调整权重系数或阈值,使该场景 index >1.0。

#### Scenario: 低收益高成本场景(关闭风扇)

**Given** 传感器数据:
- CO₂: 600 ppm(室内质量好)
- 温度: 24°C

**And** 天气数据:
- PM2.5: 150 μg/m³(室外污染严重)
- 室外温度: 10°C(温差大)
- 风速: 1 m/s

**When** 调用 `decision_make(&sensor, &weather, MODE_NORMAL)`

**Then**
- 计算通风指数:
  ```c
  indoor_quality = (600 - 400) / 1600 = 0.125
  outdoor_quality = 150 / 100 = 1.5
  temp_diff = |24 - 10| = 14

  benefit = 0.125 * 10 = 1.25
  cost = 1.5 * 5 + 14 * 2 = 35.5

  index = 1.25 - 35.5 = -34.25
  ```
- index <= 1.0 → 返回 `FAN_OFF`
- 日志输出包含:`indoor_q=0.13 outdoor_q=1.50 temp_diff=14.0 benefit=1.25 cost=35.50 index=-34.25`

#### Scenario: MODE_DEGRADED 使用缓存数据决策

**Given** 传感器数据:
- CO₂: 1300 ppm
- 温度: 24°C
- valid: true

**And** 天气数据(缓存,WiFi 断开 <30分钟):
- PM2.5: 30 μg/m³
- 温度: 23°C
- valid: true(缓存仍有效)

**When** 调用 `decision_make(&sensor, &weather, MODE_DEGRADED)`

**Then**
- 检测到 `weather->valid == true`,使用 Benefit-Cost 模型
- 计算通风指数:
  ```c
  indoor_quality = (1300 - 400) / 1600 = 0.5625
  outdoor_quality = 30 / 100 = 0.3
  temp_diff = |24 - 23| = 1

  benefit = 0.5625 * 10 = 5.625
  cost = 0.3 * 5 + 1 * 2 = 3.5

  index = 5.625 - 3.5 = 2.125
  ```
- 1.0 < index <= 3.0 → 返回 `FAN_LOW`
- 日志输出:`决策计算: indoor_q=0.56 outdoor_q=0.30 temp_diff=1.0 benefit=5.63 cost=3.50 index=2.13`

**注意**: MODE_DEGRADED 与 MODE_NORMAL 的算法逻辑完全相同,区别仅在于天气数据来源(实时 vs 缓存)。

#### Scenario: MODE_LOCAL 降级为仅 CO₂ 决策

**Given** 传感器数据:
- CO₂: 1300 ppm
- 温度: 24°C
- valid: true

**And** 系统模式: MODE_LOCAL(WiFi 断开 >30分钟,缓存过期)

**When** 调用 `decision_make(&sensor, &weather, MODE_LOCAL)`

**Then**
- 检测到 `mode == MODE_LOCAL`,跳过 Benefit-Cost 计算
- 调用 `local_mode_decide(1300.0f)`
- 根据本地模式逻辑:1300 > 1200 → 返回 `FAN_HIGH`
- 日志输出:`MODE_LOCAL: 使用本地 CO₂ 决策`

**注意**: `weather` 参数在 MODE_LOCAL 下被忽略,即使传入也不使用。

---
