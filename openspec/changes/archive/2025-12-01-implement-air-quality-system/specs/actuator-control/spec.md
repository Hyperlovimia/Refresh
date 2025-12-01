# 规范：执行器控制

## 概述

本规范定义了风扇控制模块的接口和行为，包括 PWM 初始化、状态设置和状态查询。

---

## ADDED Requirements

### Requirement: 风扇控制接口

风扇控制模块 MUST 提供初始化、状态设置和状态查询功能，通过 PWM 控制风扇转速。

#### Scenario: 初始化风扇控制

**Given** ESP32-S3 GPIO（待定）已配置为 PWM 输出
**When** 调用 `fan_control_init()`
**Then**
- LEDC PWM 配置为 25kHz 频率（避免可闻噪音）
- PWM 分辨率设置为 8 位（0-255）
- 初始状态设置为 `FAN_OFF`（占空比 0%）
- 函数返回 `ESP_OK`

#### Scenario: 设置风扇状态为关闭

**Given** 风扇控制已初始化
**When** 调用 `fan_control_set_state(FAN_OFF)`
**Then**
- PWM 占空比设置为 0%（值 0/255）
- 风扇停止转动
- 函数返回 `ESP_OK`

#### Scenario: 设置风扇状态为低速

**Given** 风扇控制已初始化
**When** 调用 `fan_control_set_state(FAN_LOW)`
**Then**
- PWM 占空比设置为 50%（值 128/255）
- 风扇以低速转动
- 函数返回 `ESP_OK`

#### Scenario: 设置风扇状态为高速

**Given** 风扇控制已初始化
**When** 调用 `fan_control_set_state(FAN_HIGH)`
**Then**
- PWM 占空比设置为 100%（值 255/255）
- 风扇以全速转动
- 函数返回 `ESP_OK`

#### Scenario: 查询风扇当前状态

**Given** 风扇状态已设置为 `FAN_LOW`
**When** 调用 `fan_control_get_state()`
**Then** 返回 `FAN_LOW`

#### Scenario: 快速切换风扇状态

**Given** 风扇状态为 `FAN_OFF`
**When** 依次调用：
1. `fan_control_set_state(FAN_HIGH)`
2. `fan_control_set_state(FAN_LOW)`
3. `fan_control_set_state(FAN_OFF)`

**Then**
- 每次调用立即生效
- 状态查询返回最后设置的状态
- 无延迟或抖动

---

## 数据结构定义

```c
// 风扇状态枚举
typedef enum {
    FAN_OFF = 0,   // 关闭（0% 占空比）
    FAN_LOW = 1,   // 低速（50% 占空比）
    FAN_HIGH = 2   // 高速（100% 占空比）
} FanState;
```

---

## 接口定义

### 风扇控制模块（actuators/fan_control.h）

```c
/**
 * @brief 初始化风扇控制（配置 PWM）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t fan_control_init(void);

/**
 * @brief 设置风扇状态
 * @param state 目标状态（FAN_OFF/FAN_LOW/FAN_HIGH）
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t fan_control_set_state(FanState state);

/**
 * @brief 获取风扇当前状态
 * @return 当前状态（FAN_OFF/FAN_LOW/FAN_HIGH）
 */
FanState fan_control_get_state(void);
```

---

## 约束条件

1. **PWM 配置**：
   - 频率：25kHz（超声频率，人耳不可闻）
   - 分辨率：8 位（0-255）
   - GPIO：需在硬件设计阶段确定（建议使用 GPIO18-21）

2. **占空比映射**：
   - `FAN_OFF` → 0% （值 0）
   - `FAN_LOW` → 50% （值 128）
   - `FAN_HIGH` → 100% （值 255）

3. **硬件要求**：
   - 风扇功率 <10W
   - 需要 MOSFET 或继电器驱动电路（ESP32 GPIO 无法直接驱动）
   - 风扇电源独立于 ESP32 电源（5V 2A 适配器）

4. **状态持久化**：
   - 状态仅存储在内存中（静态变量）
   - 系统重启后恢复为 `FAN_OFF`

5. **安全保护**：
   - 系统进入 `ERROR` 状态时，必须强制设置为 `FAN_OFF`
   - 初始化失败时，GPIO 保持低电平（风扇关闭）

---

## 测试要求

1. **单元测试**：
   - 状态设置和查询正确性
   - 占空比计算正确性

2. **硬件验证**：
   - 使用万用表或示波器测量 PWM 占空比：
     - `FAN_OFF`: 0%
     - `FAN_LOW`: 50% ± 2%
     - `FAN_HIGH`: 100%
   - 使用实际风扇验证转速变化

3. **可靠性测试**：
   - 快速切换状态 1000 次，无故障
   - 长期运行 24 小时，状态稳定

---

## 扩展性

未来可扩展为支持更细粒度的速度控制：
```c
// 扩展接口（当前不实现）
esp_err_t fan_control_set_speed(uint8_t percentage); // 0-100%
```
