# actuator-control Specification

## Purpose
TBD - created by archiving change implement-air-quality-system. Update Purpose after archive.
## Requirements
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

