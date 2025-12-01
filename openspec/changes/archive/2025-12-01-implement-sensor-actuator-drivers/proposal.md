# 实现传感器和执行器驱动（第二阶段）

## 变更 ID
`implement-sensor-actuator-drivers`

## 状态
提案阶段（Proposal）

## 背景

第一阶段（`implement-air-quality-system`）已完成系统框架和主逻辑，建立了完整的模块接口定义和任务调度架构。第一阶段修复（`fix-core-implementation-gaps`）已解决 I2C 互斥、错误恢复、MQTT 节拍等核心问题。

当前代码状态：
- **主逻辑**：main.c 完整实现，包括状态机、任务调度、错误恢复
- **传感器层**：接口定义完整，但驱动实现为桩函数
  - CO₂ 传感器：UART 已初始化，读取函数返回固定值（850.0 ppm）
  - SHT35 传感器：完全是桩实现（返回固定值 24.5°C / 58%）
- **执行器层**：风扇控制逻辑完整，但 PWM 驱动未实现，且存在编码问题（中文注释显示为"???"）

**问题**：
1. 决策算法依赖真实传感器数据，当前固定值无法触发正确的风扇控制逻辑
2. 系统无法在实际硬件上验证完整功能
3. fan_control.c 的编码问题违反 CLAUDE.md "所有文档以及注释必须用简体中文，UTF-8(无BOM)格式编写并保存"

## 问题描述

### 问题 1：CO₂ 传感器读取逻辑缺失

**现状**：
- `main/sensors/co2_sensor.c:62-66`：`co2_sensor_read_ppm()` 返回固定值 850.0f
- UART 驱动已初始化（GPIO16/17, 9600 8N1），但未实现帧接收和解析
- JX-CO2-102-5K 传感器使用主动上报模式，每秒发送 ASCII 帧：`CO2:1234\r\n`

**影响**：
- 决策算法无法响应真实 CO₂ 浓度变化
- 系统永远不会触发 FAN_HIGH 状态（需要 CO₂ > 1500 ppm）

### 问题 2：SHT35 温湿度读取逻辑缺失

**现状**：
- `main/sensors/sht35.c:17-23`：`sht35_read()` 返回固定值 24.5°C / 58%
- I2C 驱动未初始化，无法与 SHT35（地址 0x44）通信
- 需要实现 I2C 命令序列：发送测量命令 → 延迟 → 读取数据 → CRC 校验 → 温湿度计算

**影响**：
- 无法监测实际室内温湿度
- OLED 显示的温湿度数据无意义
- 决策算法的温度补偿逻辑无法生效

### 问题 3：风扇 PWM 控制未实现

**现状**：
- `main/actuators/fan_control.c:42-57`：`fan_control_init()` 和 `fan_control_set_state()` 中标记 TODO
- 逻辑层完整（PWM 范围保护、昼夜模式映射、状态管理），但未调用 LEDC 驱动
- 需要配置 LEDC：GPIO25, 25kHz, 8-bit 分辨率

**影响**：
- 风扇无法实际运转
- 系统无法实现空气质量改善的核心功能

### 问题 4：fan_control.c 编码错误

**现状**：
- `main/actuators/fan_control.c` 的所有中文注释和日志显示为"???"
- 违反 CLAUDE.md 编码规范

**影响**：
- 破坏代码可读性
- 后续开发无法依赖注释理解逻辑

## 目标

1. **实现 CO₂ 传感器读取**：解析 UART 主动上报帧，提取真实 CO₂ 浓度值
2. **实现 SHT35 读取**：初始化 I2C 总线，实现测量命令序列和数据校验
3. **实现风扇 PWM 控制**：配置 LEDC 外设，实现真实的 PWM 输出
4. **修复 fan_control.c 编码**：重新保存为 UTF-8(无BOM)，恢复中文注释

## 非目标

- 不实现传感器校准功能（预留接口已存在，实现留待后续）
- 不实现网络层、决策算法、UI 层（这些是第三、四、五阶段的任务）
- 不修改模块接口定义（所有接口在第一阶段已确定）

## 成功标准

1. **编译验证**：`idf.py build` 无错误和警告
2. **功能验证**（由用户在硬件设备上执行）：
   - CO₂ 传感器：串口日志正确显示真实 CO₂ 浓度（与手持测量仪对比）
   - SHT35 传感器：串口日志正确显示温湿度（与室温对比）
   - 风扇控制：风扇可按 OFF/LOW/HIGH 三档运行，转速符合预期
3. **代码审查**：所有实现符合 SOLID 原则和项目编码规范
4. **编码规范**：fan_control.c 中文注释正确显示

## 依赖关系

- **前置依赖**：第一阶段（系统框架）和第一阶段修复（I2C 互斥）已完成
- **无阻塞依赖**：三个驱动可以并行实现

## 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| CO₂ 传感器帧格式理解错误 | 中 | 高 | 使用 exa MCP 查找官方文档，实现前通过串口监视器验证帧格式 |
| SHT35 I2C 通信失败 | 中 | 高 | 使用 I2C 扫描工具确认设备地址，参考官方驱动示例 |
| PWM 频率设置错误导致风扇异响 | 低 | 中 | 从低频开始测试，逐步调整到 25kHz |
| 硬件连接错误 | 中 | 高 | 提供清晰的接线指引，用户在测试前确认连接 |

## 预期影响范围

### 修改文件
- `main/sensors/co2_sensor.c`：实现 `co2_sensor_read_ppm()` 的 UART 帧接收和解析
- `main/sensors/sht35.c`：实现 `sht35_init()` 和 `sht35_read()` 的 I2C 驱动逻辑
- `main/actuators/fan_control.c`：实现 PWM 初始化和占空比设置，修复编码

### 新增文件
- `openspec/changes/implement-sensor-actuator-drivers/proposal.md`：本提案
- `openspec/changes/implement-sensor-actuator-drivers/tasks.md`：任务清单
- `openspec/changes/implement-sensor-actuator-drivers/design.md`：技术设计（如需）
- `openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md`：规格增量

### 影响组件
- **传感器管理器**：无需修改（已正确调用传感器接口）
- **决策任务**：无需修改（已正确处理传感器数据）
- **主状态机**：无需修改（已正确处理预热和稳定状态）

## 替代方案

### 方案 A：使用第三方驱动库（如 esp-idf-lib）
**优点**：减少开发工作量，复用成熟代码
**缺点**：
- JX-CO2-102-5K 没有现成的库（非主流传感器）
- SHT35 有第三方库，但需要额外依赖管理
- 违反 CLAUDE.md "架构优先级"的"标准化 + 生态复用"原则（虽然鼓励复用，但对于简单驱动，自研更可控）
**决策**：不采用，保持简单自研

### 方案 B：将传感器读取改为中断驱动（而非轮询）
**优点**：降低 CPU 占用
**缺点**：增加复杂度，FreeRTOS 任务模型下轮询已足够高效
**决策**：不采用，保持轮询模式

## 向后兼容性

- 无 API 变更，所有接口保持不变
- 从桩实现切换到真实实现，调用方（传感器管理器）无感知
- NVS 存储格式不变

## 审查检查点

- [ ] 提案文档完整且清晰描述问题和目标
- [ ] 任务分解可独立验证且顺序合理
- [ ] 规格增量（如有）符合 OpenSpec 格式要求
- [ ] `openspec validate implement-sensor-actuator-drivers --strict` 无错误

## 参考资料

- 原始计划：`openspec/changes/archive/2025-12-01-implement-air-quality-system/tasks.md` 第二阶段
- ESP-IDF UART 驱动：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/uart.html
- ESP-IDF I2C 驱动：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/i2c.html
- ESP-IDF LEDC 驱动：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/ledc.html
- SHT35 数据手册：（待使用 exa MCP 查找）
- JX-CO2-102-5K 协议文档：（待使用 exa MCP 查找）
