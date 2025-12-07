# actuator-control Spec Delta

## MODIFIED Requirements

### Requirement: 多风扇控制接口

系统 MUST 支持3个独立风扇的PWM控制，使用LEDC外设生成25kHz PWM信号，每个风扇可独立设置状态。

**原规格变更点**：
- 从单风扇扩展为3风扇（Fan0/Fan1/Fan2）
- GPIO分配：Fan0=GPIO26, Fan1=GPIO27, Fan2=GPIO33
- LEDC通道：3个风扇共用TIMER_0，分别使用CHANNEL_0/1/2
- 接口函数增加FanId参数支持独立控制

#### Scenario: 初始化3个风扇的LEDC PWM外设

**Given**：
- 系统启动，LEDC未配置

**When**：
- 调用 `fan_control_init()`

**Then**：
1. 配置 LEDC 定时器（共用）：
   - 速度模式：`LEDC_LOW_SPEED_MODE`
   - 分辨率：`LEDC_TIMER_8_BIT`（0-255）
   - 频率：25kHz
   - 时钟源：`LEDC_AUTO_CLK`
2. 配置3个 LEDC 通道：
   - Fan0: GPIO26, CHANNEL_0
   - Fan1: GPIO27, CHANNEL_1
   - Fan2: GPIO33, CHANNEL_2
   - 定时器：均绑定 LEDC_TIMER_0
   - 初始占空比：0（关闭）
3. 返回 `ESP_OK`

**Acceptance Criteria**：
- LEDC 定时器和3个通道配置成功
- GPIO26/27/33 输出 PWM 波形，频率 25kHz ± 5%
- 所有风扇初始占空比为 0

#### Scenario: 独立设置单个风扇状态

**Given**：
- LEDC 已初始化
- 3个风扇均为 `FAN_OFF`

**When**：
- 调用 `fan_control_set_state(FAN_ID_1, FAN_HIGH, false)`

**Then**：
1. 仅Fan1的PWM占空比设置为255
2. Fan0和Fan2保持不变（PWM=0）
3. 返回 `ESP_OK`

**Acceptance Criteria**：
- 仅指定风扇状态改变
- 其他风扇不受影响
- 支持FAN_ID_0/1/2三个有效ID

#### Scenario: 批量设置所有风扇状态

**Given**：
- LEDC 已初始化
- 3个风扇状态各异

**When**：
- 调用 `fan_control_set_all(FAN_LOW, true)`（夜间模式）

**Then**：
1. 3个风扇PWM占空比均设置为150（夜间低速）
2. 返回 `ESP_OK`

**Acceptance Criteria**：
- 所有风扇统一设置
- 夜间模式占空比映射正确

#### Scenario: 查询单个风扇状态

**Given**：
- Fan0=OFF, Fan1=LOW, Fan2=HIGH

**When**：
- 调用 `fan_control_get_state(FAN_ID_2)`

**Then**：
- 返回 `FAN_HIGH`

**Acceptance Criteria**：
- 返回指定风扇的当前状态
- 无效ID返回FAN_OFF

#### Scenario: PWM范围保护（多风扇）

**Given**：
- 外部调用者尝试设置超出范围的PWM值

**When**：
- 调用 `fan_control_set_pwm(FAN_ID_0, 300)`

**Then**：
- 实际设置的PWM为255（限幅后）
- 仅影响Fan0

**Acceptance Criteria**：
- PWM限幅逻辑对每个风扇独立生效
- 限幅规则不变：0保持0，<150限幅150，>255限幅255
