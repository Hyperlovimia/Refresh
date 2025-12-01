# 执行器控制规格增量

## MODIFIED Requirements

### Requirement: 风扇控制接口

系统 MUST 使用 LEDC 外设生成 25kHz PWM 信号，并根据风扇状态和昼夜模式映射到正确的占空比，同时保护 PWM 范围防止风扇启动失败。

**原规格**（`openspec/specs/actuator-control/spec.md`）：
- 风扇使用 30FAN Module X1，PWM 控制（GPIO26）

**增量修改**：
明确 LEDC 配置参数（25kHz, 8-bit, LOW_SPEED_MODE）和占空比映射规则（OFF:0, LOW:150/180, HIGH:200/255）。

#### Scenario: 初始化 LEDC PWM 外设

**Given**：
- 系统启动，LEDC 未配置

**When**：
- 调用 `fan_control_init()`

**Then**：
1. 配置 LEDC 定时器：
   - 速度模式：`LEDC_LOW_SPEED_MODE`
   - 分辨率：`LEDC_TIMER_8_BIT`（0-255）
   - 频率：25kHz
   - 时钟源：`LEDC_AUTO_CLK`
2. 配置 LEDC 通道：
   - GPIO：GPIO_NUM_26
   - 通道：`LEDC_CHANNEL_0`
   - 定时器：`LEDC_TIMER_0`
   - 初始占空比：0（关闭）
3. 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ LEDC 定时器和通道配置成功
- ✅ GPIO26 输出 PWM 波形，频率 25kHz ± 5%
- ✅ 初始占空比为 0

**注意**：ESP32-S3 跳过 GPIO22-25，因此使用 GPIO20（I2C SCL）和 GPIO26（PWM）

#### Scenario: 设置风扇状态并输出 PWM

**Given**：
- LEDC 已初始化
- 当前风扇状态为 `FAN_OFF`

**When**：
- 调用 `fan_control_set_state(FAN_HIGH, false)`（白天模式）

**Then**：
1. 计算 PWM 占空比：255（100%）
2. 调用 `ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255)`
3. 调用 `ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0)`
4. 更新内部状态：`current_state = FAN_HIGH`, `current_pwm = 255`
5. 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ FAN_OFF → PWM duty = 0
- ✅ FAN_LOW (白天) → PWM duty = 180
- ✅ FAN_LOW (夜间) → PWM duty = 150
- ✅ FAN_HIGH (白天) → PWM duty = 255
- ✅ FAN_HIGH (夜间) → PWM duty = 200
- ✅ PWM 输出占空比可通过示波器测量验证

#### Scenario: PWM 范围保护

**Given**：
- 外部调用者尝试设置超出范围的 PWM 值

**When**：
- 调用 `fan_control_set_pwm(300)`

**Then**：
- `fan_clamp_pwm(300)` 限幅为 255
- 实际设置的 PWM 为 255
- 打印 DEBUG 日志：`设置 PWM：300 -> 255 (clamp 后)`

**Acceptance Criteria**：
- ✅ PWM < 150 且 != 0 → 限幅为 150（最小启动扭矩）
- ✅ PWM > 255 → 限幅为 255（最大占空比）
- ✅ PWM = 0 → 保持 0（允许关闭）
