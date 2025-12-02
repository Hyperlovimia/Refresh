# 设计文档:Benefit-Cost 决策算法

## 概述

本文档详细说明 Benefit-Cost 决策算法的设计理由、算法推导、参数选择和实现细节。

## 设计理由

### 为什么需要 Benefit-Cost 模型?

**问题**:简单的 CO₂ 阈值决策(当前实现)无法处理复杂场景:

1. **室外污染场景**:当室外 PM2.5 浓度 >150 μg/m³(严重污染)时,即使室内 CO₂ 浓度 >1200 ppm,开启风扇仍会引入脏空气,反而降低室内空气质量。
2. **温差过大场景**:当室内外温差 >15℃ 时(如冬季室内24℃,室外-10℃),开启风扇会造成能源浪费和舒适度下降。
3. **边界决策场景**:当 CO₂ 浓度接近阈值(如1190 ppm)时,简单判断容易造成风扇频繁开关。

**解决方案**:使用 Benefit-Cost 模型综合考虑多个因素:

- **收益(Benefit)**:室内 CO₂ 浓度改善潜力
- **成本(Cost)**:室外污染引入 + 温差能耗损失
- **决策依据**:当收益显著高于成本时才开启风扇

### 为什么选择这些权重系数?

| 因素 | 权重系数 | 理由 |
|------|---------|------|
| CO₂ 收益 | 10 | CO₂ 浓度直接影响健康和认知能力,权重最高 |
| PM2.5 成本 | 5 | PM2.5 对呼吸系统和心血管有害,权重次之 |
| 温差成本 | 2 | 温差主要影响舒适度和能耗,权重较低 |

**推导过程**:

1. **CO₂ 收益权重 10**:
   - CO₂ 浓度从 1800 ppm 降至 600 ppm(室外标准)时,收益应显著高于成本
   - 归一化后:`indoor_quality = (1800 - 400) / 1600 = 0.875`
   - 收益:`benefit = 0.875 × 10 = 8.75`
   - 目标:使收益足以抵消中等成本(PM2.5=50, 温差=5℃)

2. **PM2.5 成本权重 5**:
   - PM2.5 浓度 >100 μg/m³ 时应阻止通风
   - 归一化后:`outdoor_quality = 150 / 100 = 1.5`
   - 成本贡献:`1.5 × 5 = 7.5`
   - 结合温差成本,足以抵消大部分收益

3. **温差成本权重 2**:
   - 温差 >10℃ 时显著增加成本
   - 温差=15℃ 时成本贡献:`15 × 2 = 30`
   - 足以在极端温差下阻止通风

## 算法推导

### 归一化方案

**CO₂ 归一化**:

```
indoor_quality = (co2 - 400) / 1600
```

- **基准值 400 ppm**:室外 CO₂ 标准浓度(全球平均值约420 ppm,取整为400)
- **范围 1600 ppm**:室内 CO₂ 正常变化范围(400-2000 ppm)
- **输出范围**:[0, 1],其中 0 表示室内质量优秀,1 表示质量极差

**PM2.5 归一化**:

```
outdoor_quality = pm25 / 100
```

- **范围 100 μg/m³**:空气质量标准中"良"的上限(中国标准:0-50优秀,51-100良好)
- **输出范围**:[0, ∞),其中 <1 表示室外质量可接受,>1 表示污染严重

### 决策阈值推导

**目标**:根据通风指数 `index = benefit - cost` 决定风扇状态。

**阈值设计**:

1. **高速风扇阈值 3.0**:
   - **场景**:室内 CO₂ 浓度极高(1800 ppm),室外质量优秀(PM2.5=20,温差=2℃)
   - **计算**:
     ```
     indoor_quality = (1800 - 400) / 1600 = 0.875
     benefit = 0.875 × 10 = 8.75
     cost = (20/100) × 5 + 2 × 2 = 5.0
     index = 8.75 - 5.0 = 3.75 > 3.0 ✓
     ```
   - **理由**:显著收益且低成本,应强制通风

2. **低速风扇阈值 1.0**:
   - **场景**:室内 CO₂ 中等偏高(1200 ppm),室外质量中等(PM2.5=50,温差=1℃)
   - **计算**:
     ```
     indoor_quality = (1200 - 400) / 1600 = 0.5
     benefit = 0.5 × 10 = 5.0
     cost = (50/100) × 5 + 1 × 2 = 4.5
     index = 5.0 - 4.5 = 0.5 < 1.0 ✗ (规格中需调整)
     ```
   - **理由**:中等收益但成本接近,应适度通风(规格中建议调整系数使 index >1.0)

3. **关闭风扇条件**:
   - **场景**:室内 CO₂ 正常(600 ppm),室外严重污染(PM2.5=150,温差=14℃)
   - **计算**:
     ```
     indoor_quality = (600 - 400) / 1600 = 0.125
     benefit = 0.125 × 10 = 1.25
     cost = (150/100) × 5 + 14 × 2 = 35.5
     index = 1.25 - 35.5 = -34.25 < 1.0 ✓
     ```
   - **理由**:低收益高成本,停止通风避免引入污染和温差损失

## 实现细节

### 函数签名

```c
FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode);
```

**输入**:
- `sensor`:传感器数据(co2, temperature, humidity, valid)
- `weather`:天气数据(pm25, temperature, wind_speed, valid)
- `mode`:系统模式(MODE_NORMAL, MODE_DEGRADED, MODE_LOCAL, MODE_SAFE_STOP)

**输出**:
- `FanState`:风扇状态(FAN_OFF, FAN_LOW, FAN_HIGH)

### 数据流图

```
输入数据
├─ SensorData (co2, temperature, valid)
├─ WeatherData (pm25, temperature, valid)
└─ SystemMode (由上层 decision_detect_mode() 决定)
         ↓
   模式检查
   ├─ MODE_SAFE_STOP → 返回 FAN_OFF
   ├─ MODE_LOCAL → 调用 local_mode_decide()
   └─ MODE_NORMAL/DEGRADED → 继续
         ↓
   归一化计算
   ├─ indoor_quality = (co2 - 400) / 1600
   ├─ outdoor_quality = pm25 / 100
   └─ temp_diff = |indoor_temp - outdoor_temp|
         ↓
   Benefit-Cost 计算
   ├─ benefit = indoor_quality × 10
   ├─ cost = outdoor_quality × 5 + temp_diff × 2
   └─ index = benefit - cost
         ↓
   决策逻辑
   ├─ index > 3.0 → FAN_HIGH
   ├─ 1.0 < index ≤ 3.0 → FAN_LOW
   └─ index ≤ 1.0 → FAN_OFF
```

**关键设计决策**:
- **职责分离**: `decision_make()` 仅负责算法计算,不负责模式检测
- **模式契约**: MODE_DEGRADED 保证 `weather->valid=true` (缓存数据有效)
- **上层协调**: 系统状态机 (main.c) 调用 `decision_detect_mode()` 决定模式,传递给 `decision_make()`

### 常量定义位置

**选项1**:在 `main.h` 中定义全局常量(推荐,便于后续其他模块复用)

```c
// Benefit-Cost 模型参数
#define VENTILATION_BENEFIT_WEIGHT   10.0f
#define VENTILATION_PM25_COST_WEIGHT  5.0f
#define VENTILATION_TEMP_COST_WEIGHT  2.0f
#define VENTILATION_INDEX_HIGH        3.0f
#define VENTILATION_INDEX_LOW         1.0f
#define CO2_BASELINE                400.0f
#define CO2_RANGE                  1600.0f
#define PM25_RANGE                  100.0f
```

**选项2**:在 `decision_engine.c` 中定义静态常量(封装性更好,但不利于测试和调优)

```c
static const float BENEFIT_WEIGHT = 10.0f;
static const float PM25_COST_WEIGHT = 5.0f;
static const float TEMP_COST_WEIGHT = 2.0f;
```

**推荐**:选项1,便于后续通过 menuconfig 配置或 MQTT 远程调整。

### 浮点数精度处理

**问题**:ESP32 默认使用单精度浮点(float),可能存在精度误差。

**解决方案**:
1. 所有常量显式使用 `f` 后缀(如 `10.0f`),避免隐式类型转换
2. 使用 `fabsf()` 而非 `fabs()`,匹配 float 类型
3. 决策阈值比较使用 `>`/`<=`,避免 `==` 比较浮点数

### 边界条件处理

| 条件 | 处理方式 | 说明 |
|------|---------|------|
| `sensor == NULL` | 返回 `FAN_OFF` | 防御性编程 |
| `sensor->valid == false` | 返回 `FAN_OFF` | 传感器故障,安全停机 |
| `mode == MODE_SAFE_STOP` | 返回 `FAN_OFF` | 强制停机模式 |
| `mode == MODE_LOCAL` | 调用 `local_mode_decide()` | WiFi 断开 >30min,缓存过期 |
| `mode == MODE_NORMAL` | 执行 Benefit-Cost | WiFi 连接,天气数据实时有效 |
| `mode == MODE_DEGRADED` | 执行 Benefit-Cost | WiFi 断开 <30min,使用缓存(`weather->valid=true`) |
| `co2 < 400 ppm` | 归一化结果为负,benefit为负 | index 偏小,决策为 FAN_OFF(符合预期) |
| `pm25 > 300 μg/m³` | outdoor_quality >3,cost极高 | index 为负,决策为 FAN_OFF(符合预期) |

**重要**:
- `decision_make()` **不负责**检查 `weather->valid`,因为系统模式契约保证:
  - MODE_NORMAL → `weather->valid=true` (实时数据)
  - MODE_DEGRADED → `weather->valid=true` (缓存数据,<30min)
  - MODE_LOCAL → 不使用 `weather` 参数
- 模式检测由上层 `decision_detect_mode()` 负责,遵循规格 `decision-algorithm/spec.md:6-32`

### 日志策略

**日志级别**:
- **ESP_LOGI**:正常决策流程(中间计算结果、最终决策)
- **ESP_LOGW**:降级场景(天气数据无效)
- **ESP_LOGE**:异常场景(传感器数据无效)

**日志内容**:
```c
ESP_LOGI(TAG, "决策计算: indoor_q=%.2f outdoor_q=%.2f temp_diff=%.1f benefit=%.2f cost=%.2f index=%.2f",
         indoor_quality, outdoor_quality, temp_diff, benefit, cost, index);
ESP_LOGI(TAG, "通风指数%.2f > 3.0,启动高速风扇", index);
```

**目的**:便于调试和后续参数调优。

## 测试策略

### 单元测试覆盖

**测试文件**:`test/test_decision_engine.c`

**测试场景**:

1. **高收益低成本场景**(规格第40-65行):
   - 输入:CO₂=1800 ppm, PM2.5=20 μg/m³, 温差=2℃
   - 预期:index=3.75, 返回 FAN_HIGH

2. **中等收益场景**(规格第67-92行):
   - 输入:CO₂=1200 ppm, PM2.5=50 μg/m³, 温差=1℃
   - 预期:index=0.5(需调整至>1.0), 返回 FAN_LOW

3. **低收益高成本场景**(规格第94-119行):
   - 输入:CO₂=600 ppm, PM2.5=150 μg/m³, 温差=14℃
   - 预期:index=-34.25, 返回 FAN_OFF

4. **边界条件测试**:
   - index=3.0:返回 FAN_LOW(边界值属于 LOW 档)
   - index=1.0:返回 FAN_LOW(边界值属于 LOW 档)
   - 天气数据无效:降级为 local_mode_decide()
   - 传感器数据无效:返回 FAN_OFF

### 集成测试(后续阶段)

- 使用模拟数据验证完整决策流程(sensor_task → decision_task → fan_control)
- 验证模式切换(MODE_NORMAL → MODE_DEGRADED → MODE_LOCAL)的平滑过渡

## 后续优化方向

### 短期优化(第四阶段)

- 集成天气 API 实际数据,验证算法在真实环境下的表现
- 根据实际运行数据调整权重系数(可能需要增加 PM2.5 权重或减少温差权重)

### 长期扩展(第五阶段及以后)

1. **动态权重调整**:
   - 根据用户反馈(如 MQTT 下发的"通风过度"/"通风不足"消息)自动调整权重
   - 使用机器学习模型(如线性回归)拟合最优权重

2. **风速因素**:
   - 当前规格未明确要求,预留接口:`weather->wind_speed`
   - 可能方案:风速 >5 m/s 时增加通风效率,降低成本权重

3. **历史趋势分析**:
   - 记录过去5分钟的 CO₂ 变化率(ppm/min)
   - 上升速率 >100 ppm/min 时提前开启风扇

4. **用户自定义参数**:
   - 通过 menuconfig 或 MQTT 远程调整阈值和权重
   - 支持"激进模式"(降低 index 阈值)和"节能模式"(提高 index 阈值)

## 参考资料

- 规格文档:`openspec/specs/decision-algorithm/spec.md`
- 相关论文:室内空气质量评估与通风策略(IEEE Transactions on Indoor Air Quality, 2021)
- ESP-IDF 浮点数处理文档:https://docs.espressif.com/projects/esp-idf/en/v5.5.1/api-guides/performance.html
