# decision-algorithm Spec Delta

## REMOVED Requirements

### Requirement: 完整决策算法（Benefit-Cost 模型）

决策引擎在 MODE_NORMAL 和 MODE_DEGRADED 模式下不再使用 Benefit-Cost 模型。

**移除原因**: 决策逻辑迁移至远程服务器，本地设备只执行远程命令。

---

## MODIFIED Requirements

### Requirement: 运行模式检测

决策引擎 MUST 根据传感器状态和 WiFi 连接状态，自动检测并切换运行模式。

**变更说明**: 移除 `cache_valid` 参数依赖，简化为三种模式。

#### Scenario: 检测远程模式

**Given** 传感器正常工作 AND WiFi 已连接
**When** 调用 `decision_detect_mode(wifi_ok=true, sensor_ok=true)`
**Then** 返回 `MODE_REMOTE`

#### Scenario: 检测本地自主模式

**Given** 传感器正常工作 AND WiFi 断开
**When** 调用 `decision_detect_mode(wifi_ok=false, sensor_ok=true)`
**Then** 返回 `MODE_LOCAL`

#### Scenario: 检测安全停机模式

**Given** 传感器故障
**When** 调用 `decision_detect_mode(wifi_ok=*, sensor_ok=false)`
**Then** 返回 `MODE_SAFE_STOP`

---

## ADDED Requirements

### Requirement: 远程命令执行

决策引擎在 MODE_REMOTE 模式下 MUST 直接执行远程服务器下发的风扇控制命令。

#### Scenario: 执行远程关闭命令

**Given** 系统处于 MODE_REMOTE
**And** 远程命令为 `FAN_OFF`
**When** 调用 `decision_make(&sensor, MODE_REMOTE, FAN_OFF)`
**Then** 返回 `FAN_OFF`

#### Scenario: 执行远程低速命令

**Given** 系统处于 MODE_REMOTE
**And** 远程命令为 `FAN_LOW`
**When** 调用 `decision_make(&sensor, MODE_REMOTE, FAN_LOW)`
**Then** 返回 `FAN_LOW`

#### Scenario: 执行远程高速命令

**Given** 系统处于 MODE_REMOTE
**And** 远程命令为 `FAN_HIGH`
**When** 调用 `decision_make(&sensor, MODE_REMOTE, FAN_HIGH)`
**Then** 返回 `FAN_HIGH`

#### Scenario: 远程命令无效时保持当前状态

**Given** 系统处于 MODE_REMOTE
**And** 未收到有效远程命令
**When** 调用 `decision_make(&sensor, MODE_REMOTE, FAN_INVALID)`
**Then** 返回上一次有效的风扇状态

---

### Requirement: 本地自主决策模式（保持不变）

决策引擎在 MODE_LOCAL 模式下 MUST 使用简化的本地决策算法，仅基于 CO₂ 浓度。

#### Scenario: CO₂ 极高（启动高速风扇）

**Given** CO₂ 浓度为 1800 ppm
**When** 调用 `local_mode_decide(1800.0f)`
**Then** 返回 `FAN_HIGH`

#### Scenario: CO₂ 偏高（启动低速风扇）

**Given** CO₂ 浓度为 1100 ppm
**When** 调用 `local_mode_decide(1100.0f)`
**Then** 返回 `FAN_LOW`

#### Scenario: CO₂ 正常（关闭风扇）

**Given** CO₂ 浓度为 800 ppm
**When** 调用 `local_mode_decide(800.0f)`
**Then** 返回 `FAN_OFF`

---

### Requirement: 安全停机模式（保持不变）

决策引擎在 MODE_SAFE_STOP 模式下 MUST 强制关闭风扇。

#### Scenario: 传感器故障时关闭风扇

**Given** 传感器故障
**When** 调用 `decision_make(&sensor, MODE_SAFE_STOP, *)`
**Then** 返回 `FAN_OFF`
