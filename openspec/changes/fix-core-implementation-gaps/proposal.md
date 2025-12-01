# 修复核心实现缺口

## 变更 ID
`fix-core-implementation-gaps`

## 状态
提案阶段（Proposal）

## 背景

在 2025-12-01 的 Codex 审查中，发现当前实现存在 4 个严重偏离规格的问题：

1. **高危**：缺少 `i2c_mutex` 保护 I2C 总线访问，导致 SHT35 传感器和 OLED 显示器可能同时操作总线，引发竞争风险
2. **高危**：错误恢复流程不完整，传感器故障恢复后状态机卡在 `STATE_INIT`，无法重新进入正常运行状态
3. **中危**：MQTT 状态上报频率违背 30 秒节拍要求，实际仅每 10 分钟上报一次
4. **低危**：多个源文件的中文注释和日志出现"???"乱码，违反 UTF-8 编码规范

Codex 综合评分 43/100，建议退回修复。

## 问题描述

### 问题 1：缺少 I2C 总线互斥保护

**现状**：
- 规格 `openspec/specs/system-orchestration/spec.md:205-229` 明确要求创建 `i2c_mutex` 并在访问 I2C 设备时加锁
- 代码 `main/main.c:43-46` 只声明和创建了 `data_mutex`、`system_events`、`alert_queue`
- SHT35 读取和 OLED 刷新没有任何互斥保护

**风险**：
- 传感器任务和显示任务可能同时访问 I2C 总线
- 导致 I2C 驱动崩溃、数据损坏或设备挂死
- 严重影响系统稳定性

### 问题 2：错误恢复流程不完整

**现状**：
- 规格 `openspec/specs/system-orchestration/spec.md:237-246` 要求传感器恢复后"重新初始化并返回正常流程"
- `handle_error_state()` 检测到传感器恢复后仅调用 `state_transition(STATE_INIT)`
- `handle_init_state()` 为空函数，`system_init()` 只在 `app_main()` 首次调用
- 导致状态机停留在 `INIT` 状态，`decision_task()` 被 `current_state < STATE_RUNNING` 条件永久阻塞

**风险**：
- 系统无法从传感器故障中自动恢复
- 需要手动重启设备才能恢复正常运行
- 违背"自愈系统"的设计初衷

### 问题 3：MQTT 发布频率不符合规格

**现状**：
- 设计文档 `openspec/changes/archive/2025-12-01-implement-air-quality-system/design.md:581-583` 要求"30秒周期或状态变化时"发布
- `main/main.h:95` 定义了 `MQTT_PUBLISH_INTERVAL_SEC=30`，但该常量未被使用
- `network_task()` 仅按 `WEATHER_FETCH_INTERVAL_SEC=600` 循环，MQTT 上报实际每 10 分钟执行一次

**影响**：
- 远程监控系统无法及时获取设备状态
- 可能触发后台告警或误判设备离线

### 问题 4：源文件编码错误

**现状**：
- `main/sensors/co2_sensor.c`、`main/network/wifi_manager.c`、`main/ui/oled_display.c` 等文件的中文注释显示为"???"
- 违反 CLAUDE.md "所有文档以及注释必须用简体中文，UTF-8(无BOM)格式编写并保存"

**影响**：
- 破坏代码可读性和文档完整性
- 后续开发无法依赖这些接口说明

## 目标

1. **补全 I2C 互斥保护**：创建 `i2c_mutex` 并在所有 I2C 访问处加锁，确保总线安全
2. **实现完整错误恢复**：设计 `system_recover()` 函数，在传感器恢复后重新初始化关键模块并正确重启状态机
3. **修正 MQTT 发布节拍**：拆分网络任务的定时逻辑，确保状态消息每 30 秒上报一次
4. **修复编码问题**：重新保存源文件为 UTF-8(无BOM) 格式，恢复中文注释

## 非目标

- 不改变现有功能逻辑或业务规则
- 不引入新的外部依赖或框架
- 不修改规格定义（这是实现修复，非需求变更）

## 成功标准

1. **编译验证**：`idf.py build` 无错误和警告
2. **代码审查**：所有修改点符合 SOLID 原则和项目编码规范
3. **规格符合性**：所有 Codex 指出的问题得到修复，并能通过规格中的场景验证
4. **可追溯性**：修改记录在 `.claude/operations-log.md`，关键设计在 `design.md` 中说明

## 依赖关系

- 无前置依赖（当前系统已完成初始实现）
- 无阻塞依赖（所有修复可独立完成）

## 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| I2C 加锁引入死锁 | 中 | 高 | 使用 `portMAX_DELAY` 确保最终获取锁；审查锁持有时间 |
| 恢复流程引入新 bug | 中 | 中 | 渐进式实现，每步验证状态转换正确性 |
| MQTT 定时器干扰现有任务 | 低 | 低 | 复用 FreeRTOS 软件定时器，避免创建新任务 |
| 编码修复破坏文件内容 | 低 | 低 | 使用 Git 对比确认修改范围 |

## 预期影响范围

### 修改文件
- `main/main.c`：增加 `i2c_mutex` 声明/创建，实现 `system_recover()`，重构网络任务定时逻辑
- `main/main.h`：可能需要导出恢复函数接口（如其他模块需调用）
- `main/sensors/co2_sensor.c`：重新保存为 UTF-8
- `main/network/wifi_manager.c`：重新保存为 UTF-8
- `main/ui/oled_display.c`：重新保存为 UTF-8

### 新增文件
- `openspec/changes/fix-core-implementation-gaps/design.md`：设计文档
- `openspec/changes/fix-core-implementation-gaps/tasks.md`：任务分解
- `openspec/changes/fix-core-implementation-gaps/specs/system-orchestration/spec.md`：规格增量（如有必要）

### 影响组件
- **传感器管理器**：需在 SHT35 读取前后加锁
- **显示管理器**：需在 OLED 刷新前后加锁
- **网络任务**：拆分天气轮询和 MQTT 发布的定时逻辑
- **主状态机**：补充错误恢复时的初始化流程

## 替代方案

### 方案 A：使用事件驱动替代定时器（MQTT 发布）
**优点**：更灵活，可响应状态变化立即发布
**缺点**：需要重构网络任务架构，超出当前修复范围
**决策**：不采用，保持简单性

### 方案 B：在每个 I2C 驱动内部实现锁（而非全局锁）
**优点**：封装性更好
**缺点**：需要修改多个驱动文件，且无法保证第三方库遵循约定
**决策**：不采用，全局锁更可靠

## 向后兼容性

- 无 API 变更，不影响外部调用者
- 状态机行为变更（错误恢复后能正确重启），但符合原始设计意图
- NVS 存储格式不变

## 审查检查点

- [ ] 提案文档完整且清晰描述问题和目标
- [ ] 设计文档说明了关键实现细节和权衡
- [ ] 任务分解可独立验证且顺序合理
- [ ] 规格增量（如有）符合 OpenSpec 格式要求
- [ ] `openspec validate fix-core-implementation-gaps --strict` 无错误

## 参考资料

- Codex 审查报告：`.claude/review-report.md`
- Codex 分析上下文：`.claude/context-codex.md`
- 系统编排规格：`openspec/specs/system-orchestration/spec.md`
- 原始设计文档：`openspec/changes/archive/2025-12-01-implement-air-quality-system/design.md`
