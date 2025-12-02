# 提案:实现 Benefit-Cost 决策算法

## Why

当前 `decision_engine.c` 中的决策算法仅基于 CO₂ 阈值做简单判断(第29-38行),未考虑室外空气质量、温差和风速因素,导致以下问题:

1. **盲目通风风险**:当室外 PM2.5 严重污染时仍然开启风扇,引入脏空气反而降低室内质量
2. **能源浪费**:当室内外温差过大时仍然强制通风,造成空调/暖气能耗增加
3. **违反规格要求**:规格 `decision-algorithm/spec.md` 第36行明确要求"MUST 使用完整的 Benefit-Cost 模型计算通风指数",但当前实现仅为 TODO 注释

## 概述

本提案实现规格中定义的 Benefit-Cost 决策模型,通过综合考虑室内 CO₂ 浓度(收益因素)、室外 PM2.5 浓度和温差(成本因素),计算通风指数并智能决定风扇状态。

## 目标

- **核心功能**:实现完整的 Benefit-Cost 模型,替换当前的简单 CO₂ 阈值判断
- **算法准确性**:严格遵循规格中的计算公式和决策阈值(index > 3.0 → HIGH, > 1.0 → LOW, ≤ 1.0 → OFF)
- **鲁棒性**:处理天气数据无效(MODE_DEGRADED/MODE_LOCAL)和传感器故障(MODE_SAFE_STOP)的降级场景
- **可测试性**:提供单元测试覆盖三个规格场景(高收益低成本、中等收益、低收益高成本)

## 范围

### 包含功能

1. **Benefit-Cost 计算逻辑**
   - 室内质量指数:`indoor_quality = (co2 - 400) / 1600`,归一化到 [0, 1]
   - 室外质量指数:`outdoor_quality = pm25 / 100`,归一化到 [0, ∞)
   - 温差:`temp_diff = |indoor_temp - outdoor_temp|`,单位摄氏度
   - 收益:`benefit = indoor_quality × 10`(权重系数)
   - 成本:`cost = outdoor_quality × 5 + temp_diff × 2`(PM2.5权重5,温差权重2)
   - 通风指数:`index = benefit - cost`

2. **决策阈值逻辑**
   - `index > 3.0` → `FAN_HIGH`(高收益低成本,强制通风)
   - `1.0 < index ≤ 3.0` → `FAN_LOW`(中等收益,适度通风)
   - `index ≤ 1.0` → `FAN_OFF`(低收益高成本,停止通风)

3. **运行模式处理**
   - MODE_NORMAL:使用完整 Benefit-Cost 模型(WiFi 连接,天气数据实时有效)
   - MODE_DEGRADED:使用完整 Benefit-Cost 模型(WiFi 断开 <30分钟,使用缓存的天气数据,`weather->valid=true`)
   - MODE_LOCAL:降级为本地 CO₂ 阈值决策(WiFi 断开 >30分钟,缓存过期,不使用天气数据)
   - MODE_SAFE_STOP:强制关闭风扇(传感器故障)

4. **日志和调试支持**
   - 记录每次决策的中间计算结果(indoor_quality, outdoor_quality, benefit, cost, index)
   - 记录最终决策理由(如"通风指数3.75 > 3.0,启动高速风扇")

### 不包含功能(后续扩展)

- 动态权重调整(当前使用固定权重:benefit权重10,PM2.5权重5,温差权重2)
- 历史数据趋势分析(如CO₂上升速率)
- 用户自定义决策参数(如通过MQTT远程调整阈值)
- 风速因素(当前规格未明确要求,预留接口)

## 技术方案

### 实现位置

修改文件:`main/algorithm/decision_engine.c`(第29-38行,替换 TODO 部分)

### 算法伪代码

```c
FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode) {
    // 1. 安全检查
    if (!sensor || !sensor->valid) return FAN_OFF;
    if (mode == MODE_SAFE_STOP) return FAN_OFF;
    if (mode == MODE_LOCAL) return local_mode_decide(sensor->co2);

    // 2. MODE_NORMAL/MODE_DEGRADED: 使用 Benefit-Cost 模型
    // 注意: MODE_DEGRADED 时 weather->valid=true (缓存数据有效)
    // 系统模式由上层 decision_detect_mode() 决定,算法层仅执行计算

    // 3. 计算 Benefit-Cost 指数
    float indoor_quality = (sensor->co2 - 400.0f) / 1600.0f;  // 归一化
    float outdoor_quality = weather->pm25 / 100.0f;
    float temp_diff = fabsf(sensor->temperature - weather->temperature);

    float benefit = indoor_quality * 10.0f;  // 收益权重
    float cost = outdoor_quality * 5.0f + temp_diff * 2.0f;  // 成本权重
    float index = benefit - cost;

    // 4. 日志记录
    ESP_LOGI(TAG, "决策计算: indoor_q=%.2f outdoor_q=%.2f temp_diff=%.1f benefit=%.2f cost=%.2f index=%.2f",
             indoor_quality, outdoor_quality, temp_diff, benefit, cost, index);

    // 5. 决策
    if (index > 3.0f) {
        ESP_LOGI(TAG, "通风指数%.2f > 3.0,启动高速风扇", index);
        return FAN_HIGH;
    } else if (index > 1.0f) {
        ESP_LOGI(TAG, "通风指数%.2f > 1.0,启动低速风扇", index);
        return FAN_LOW;
    } else {
        ESP_LOGI(TAG, "通风指数%.2f <= 1.0,关闭风扇", index);
        return FAN_OFF;
    }
}
```

### 常量定义

在 `main.h` 中新增常量(或在 `decision_engine.c` 中定义静态常量):

```c
// Benefit-Cost 模型参数
#define VENTILATION_BENEFIT_WEIGHT   10.0f  ///< 收益权重系数
#define VENTILATION_PM25_COST_WEIGHT  5.0f  ///< PM2.5 成本权重
#define VENTILATION_TEMP_COST_WEIGHT  2.0f  ///< 温差成本权重

// 决策阈值
#define VENTILATION_INDEX_HIGH  3.0f  ///< 高速风扇阈值
#define VENTILATION_INDEX_LOW   1.0f  ///< 低速风扇阈值

// 归一化基准值
#define CO2_BASELINE  400.0f   ///< CO₂ 基准浓度(ppm,室外标准)
#define CO2_RANGE     1600.0f  ///< CO₂ 归一化范围(400-2000ppm)
#define PM25_RANGE    100.0f   ///< PM2.5 归一化范围(μg/m³)
```

### 依赖关系

**外部依赖**:
- `<math.h>`:使用 `fabsf()` 计算温度绝对差值
- `SensorData`:需要 `co2`(ppm) 和 `temperature`(℃) 字段
- `WeatherData`:需要 `pm25`(μg/m³) 和 `temperature`(℃) 字段

**内部依赖**:
- `local_mode_decide()`:MODE_LOCAL 模式下的降级函数(已实现)

## 实施计划

### 第三阶段:决策算法实现(本次)

**目标**:完成 Benefit-Cost 模型实现,通过单元测试验证算法正确性。

**交付物**:
- 修改 `decision_engine.c`,实现完整算法(替换第29-38行 TODO)
- (可选)在 `main.h` 中新增算法常量定义
- 编写单元测试 `test/test_decision_engine.c`,覆盖三个规格场景
- 编译通过(`idf.py build`)

**验证**:
- 单元测试覆盖规格中的三个场景(高收益低成本、中等收益、低收益高成本)
- 算法输出与规格示例一致(误差 <0.01)
- 日志输出包含完整的中间计算结果

## 依赖关系

### 实施依赖

- **前置条件**:第二阶段(传感器和执行器驱动)已完成,传感器数据可正常读取
- **阻塞风险**:若天气数据接口未实现,可使用模拟数据进行单元测试

### 数据依赖

- `SensorData` 结构必须包含:`co2`(float), `temperature`(float), `valid`(bool)
- `WeatherData` 结构必须包含:`pm25`(float), `temperature`(float), `valid`(bool)

## 风险和缓解措施

### 技术风险

1. **天气数据缺失导致算法无法执行**
   - 缓解:在天气数据无效时自动降级为 `local_mode_decide()`,确保系统可用

2. **浮点数精度误差**
   - 缓解:使用 `float` 类型足够覆盖精度需求(±0.01),关键计算使用 `fabsf()` 避免负数

3. **权重参数不合理**
   - 缓解:初期使用规格中的示例权重(benefit 10, PM2.5成本 5, 温差成本 2),后续可通过实际运行数据调优

### 实施风险

1. **单元测试覆盖不足**
   - 缓解:严格覆盖规格中的三个场景,并新增边界条件测试(如 index=3.0, index=1.0)

2. **与现有代码冲突**
   - 缓解:`local_mode.c` 已实现本地模式决策,不需要修改;仅修改 `decision_engine.c` 第29-38行

## 成功标准

### 第三阶段(本次)

- [x] `decision_engine.c` 实现完整的 Benefit-Cost 模型
- [x] 代码编译通过,无警告
- [x] 单元测试覆盖规格中的三个场景,测试通过
- [x] 日志输出包含中间计算结果(indoor_quality, benefit, cost, index)
- [x] 算法输出与规格示例一致(误差 <0.01)

### 验收条件

- [ ] 技术方案审查通过(算法公式、权重系数、决策阈值)
- [ ] 实施计划确认(交付物、验证方式)
- [ ] 单元测试计划审查通过(覆盖场景、断言条件)

## 参考资料

- 决策算法规格:`openspec/specs/decision-algorithm/spec.md`(第36-119行,Benefit-Cost 模型)
- 现有实现:`main/algorithm/decision_engine.c`(第13-39行,待实现部分)
- 数据结构定义:`main/main.h`(第56-73行,SensorData 和 WeatherData)
- 本地模式实现:`main/algorithm/local_mode.c`(已完成,作为降级方案)
