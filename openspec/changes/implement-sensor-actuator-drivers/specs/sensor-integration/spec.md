# 传感器集成规格增量

## MODIFIED Requirements

### Requirement: CO₂ 传感器数据采集

系统 MUST 从 UART 接收并解析 CO₂ 传感器的主动上报帧，提取有效的浓度值并验证数据范围。

**原规格**（`openspec/specs/sensor-integration/spec.md`）：
- CO₂ 传感器使用 JX-CO2-102-5K，UART 接口（GPIO16/17），主动上报模式

**增量修改**：
明确 UART 帧格式（`CO2:xxxx\r\n`）和数据解析规则（范围：400-5000 ppm）。

#### Scenario: 接收并解析 CO₂ 浓度帧

**Given**：
- CO₂ 传感器已通过 UART（9600 8N1）连接
- `co2_sensor_init()` 已成功初始化 UART 驱动

**When**：
- 调用 `co2_sensor_read_ppm()`

**Then**：
- 从 UART 缓冲区读取数据，查找完整帧：`CO2:xxxx\r\n`
- 提取数值部分（xxxx），转换为 float
- 验证数值范围：400-5000 ppm
- 返回有效的 CO₂ 浓度值

**Acceptance Criteria**：
- ✅ 正常帧（如 `CO2:850\r\n`）返回 850.0f
- ✅ 边界值（如 `CO2:400\r\n`、`CO2:5000\r\n`）正确解析
- ✅ 超出范围值（如 `CO2:50\r\n`、`CO2:9999\r\n`）返回 -1.0f 并打印 WARN 日志
- ✅ 无效格式（如 `CO2ABC\r\n`）返回 -1.0f
- ✅ UART 无数据（超时）返回 -1.0f

---

### Requirement: SHT35 温湿度数据采集

系统 MUST 通过 I2C 发送测量命令、读取数据、验证 CRC-8 校验和，并将原始值转换为温湿度物理值。

**原规格**（`openspec/specs/sensor-integration/spec.md`）：
- SHT35 传感器使用 I2C 接口（GPIO21/22），地址 0x44

**增量修改**：
明确 I2C 命令序列（单次测量 0x2C 0x06）、CRC-8 校验（多项式 0x31）和转换公式。

#### Scenario: 初始化 I2C 总线并配置 SHT35

**Given**：
- 系统启动，I2C 总线未初始化

**When**：
- 调用 `sht35_init()`

**Then**：
- 配置 I2C 主机模式：SDA=GPIO21, SCL=GPIO22, 速率 100kHz
- 安装 I2C 驱动
- 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ I2C 驱动初始化成功
- ✅ 可通过 `i2cdetect` 扫描到设备地址 0x44
- ✅ 防止重复初始化（第二次调用返回 `ESP_OK` 但不重新配置）

#### Scenario: 读取温湿度并验证 CRC

**Given**：
- I2C 总线已初始化
- SHT35 传感器已连接

**When**：
- 调用 `sht35_read(&temp, &humi)`

**Then**：
1. 获取 I2C 互斥锁（超时 100ms）
2. 发送单次测量命令（高重复性）：`0x2C 0x06`
3. 延迟 15ms（等待测量完成）
4. 读取 6 字节数据：
   - 字节 0-1：温度原始值（MSB 在前）
   - 字节 2：温度 CRC-8
   - 字节 3-4：湿度原始值（MSB 在前）
   - 字节 5：湿度 CRC-8
5. 验证两个 CRC-8 校验和（多项式：0x31，初值：0xFF）
6. 转换原始值到物理值：
   - 温度 = -45 + 175 * (raw_temp / 65535.0)
   - 湿度 = 100 * (raw_humi / 65535.0)
7. 释放 I2C 互斥锁
8. 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ 温度读数在合理范围（-10°C ~ 60°C）
- ✅ 湿度读数在合理范围（0% ~ 100%）
- ✅ CRC 校验失败时返回 `ESP_ERR_INVALID_CRC` 并打印 WARN 日志
- ✅ I2C 通信超时时返回 `ESP_ERR_TIMEOUT` 并打印 ERROR 日志
- ✅ 互斥锁获取失败时返回 `ESP_ERR_TIMEOUT` 并打印 ERROR 日志

---

### Requirement: 风扇 PWM 控制

系统 MUST 使用 LEDC 外设生成 25kHz PWM 信号，并根据风扇状态和昼夜模式映射到正确的占空比，同时保护 PWM 范围防止风扇启动失败。

**原规格**（`openspec/specs/actuator-control/spec.md`）：
- 风扇使用 30FAN Module X1，PWM 控制（GPIO25）

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
   - GPIO：GPIO_NUM_25
   - 通道：`LEDC_CHANNEL_0`
   - 定时器：`LEDC_TIMER_0`
   - 初始占空比：0（关闭）
3. 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ LEDC 定时器和通道配置成功
- ✅ GPIO25 输出 PWM 波形，频率 25kHz ± 5%
- ✅ 初始占空比为 0

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

---

## ADDED Requirements

### Requirement: 编码规范符合性

所有源文件 MUST 使用 UTF-8(无BOM) 编码，确保中文注释和日志字符串正确显示，符合 CLAUDE.md 编码规范。

#### Scenario: 源文件编码检查

**Given**：
- 所有源文件已编写完成

**When**：
- 使用 `file` 命令检查编码

**Then**：
- 所有 `.c` 和 `.h` 文件显示 `UTF-8 Unicode text`
- 所有中文注释和字符串正确显示，无"???"乱码

**Acceptance Criteria**：
- ✅ `main/actuators/fan_control.c` 编码为 UTF-8
- ✅ 所有中文注释在源代码编辑器中正确显示
- ✅ 串口日志中的中文字符正确显示（ESP-IDF 支持 UTF-8）

---

## 验证方法

### 单元测试（可选）
- CO₂ 帧解析：模拟 UART 数据流，验证解析逻辑
- SHT35 CRC 校验：使用已知的原始数据和 CRC 值验证算法
- PWM 范围保护：测试 `fan_clamp_pwm()` 的边界条件

### 集成测试（硬件）
- 传感器读数准确性：对比手持测量仪或参考温度计
- 风扇响应速度：触发高 CO₂ 状态，观察风扇启动时间（<1秒）
- 错误恢复：断开传感器连接，验证系统自动恢复
