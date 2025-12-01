# 系统编排 - 规格增量（修复实现偏差）

本增量文件用于标记本次修复涉及的规格需求，所有需求本身在 `openspec/specs/system-orchestration/spec.md` 中已完整定义，本文档仅明确修复范围。

---

## MODIFIED Requirements

### Requirement: 同步机制

系统 MUST 使用互斥锁和事件组保护共享资源和同步任务。

**变更说明**：本次修复补全了规格要求但实现中遗漏的 I2C 互斥锁机制。

#### Scenario: 创建互斥锁和事件组

**Given** 系统初始化阶段
**When** 调用 `app_main()`
**Then** 创建：
- `data_mutex`：保护共享数据缓冲区（sensor_data、weather_data、fan_state）
- `i2c_mutex`：保护 I2C 总线访问（SHT35 + OLED）✅ **本次修复：补全实现**
- `system_events`：事件组（WiFi 连接、传感器就绪等事件）

**实施内容**：
- 在 `main/main.c` 中声明 `static SemaphoreHandle_t i2c_mutex`
- 在 `system_init()` 中调用 `xSemaphoreCreateMutex()` 创建锁
- 导出 `get_i2c_mutex()` 接口供其他模块使用

---

#### Scenario: I2C 总线互斥访问

**Given** 传感器任务需要读取 SHT35，显示任务需要更新 OLED
**When** 两个任务同时执行
**Then**
- 传感器任务：✅ **本次修复：在 sensor_manager.c 中添加锁保护**
  ```c
  xSemaphoreTake(i2c_mutex, portMAX_DELAY);
  sht35_read(&temp, &humi);
  xSemaphoreGive(i2c_mutex);
  ```
- 显示任务：✅ **本次修复：在 display_manager.c 中添加锁保护**
  ```c
  xSemaphoreTake(i2c_mutex, portMAX_DELAY);
  oled_display_main_page(...);
  xSemaphoreGive(i2c_mutex);
  ```
- 保证 I2C 操作不冲突

**实施内容**：
- 修改 `main/sensors/sensor_manager.c` 的 SHT35 读取逻辑，增加锁保护
- 修改 `main/ui/display_manager.c` 的 OLED 刷新逻辑，增加锁保护
- 锁超时设为 100ms，避免永久阻塞

---

### Requirement: 错误处理

系统 MUST 处理各种错误情况并采取安全措施，包括传感器故障恢复后的完整重新初始化流程。

**变更说明**：本次修复补全了传感器故障恢复后的重新初始化流程，确保状态机能正确重启。

#### Scenario: 传感器读取连续失败

**Given** 传感器读取连续失败 3 次
**When** `sensor_manager_is_healthy()` 返回 `false`
**Then**
- 主任务检测到传感器故障
- 状态转换为 `STATE_ERROR`
- 调用 `fan_control_set_state(FAN_OFF)` 安全停机
- 每 10 秒重试一次传感器初始化 ✅ **本次修复：实现完整恢复流程**

**实施内容**：
- 在 `handle_error_state()` 中检测传感器恢复后调用 `sensor_manager_reinit()`
- 新增 `sensor_manager_reinit()` 函数，清空缓冲区和计数器，重新初始化传感器
- 实现 `handle_init_state()` 函数，在恢复后重新进入 `STATE_PREHEATING`
- 确保状态机完整路径：`ERROR` → `INIT` → `PREHEATING` → `STABILIZING` → `RUNNING`

---

#### Scenario: 传感器恢复后重新初始化（新增明确说明）

**Given** 系统处于 `STATE_ERROR`，传感器已恢复健康
**When** `handle_error_state()` 检测到 `sensor_manager_is_healthy()` 返回 `true`
**Then**
- 调用 `sensor_manager_reinit()` 清空传感器状态
- 清除 `EVENT_SENSOR_FAULT` 事件标志
- 转换到 `STATE_INIT`
- `handle_init_state()` 检测到恢复场景，立即转换到 `STATE_PREHEATING`
- 经过 60 秒预热和 240 秒稳定后，进入 `STATE_RUNNING`
- 决策任务恢复正常执行

**实施内容**：
- 修改 `main/main.c` 的 `handle_error_state()` 恢复逻辑
- 实现 `handle_init_state()` 区分首次启动和错误恢复场景
- 新增 `main/sensors/sensor_manager.c` 的 `sensor_manager_reinit()` 函数
- 验证状态转换时序和决策任务恢复

---

## 实施说明

本次修复严格遵循现有规格定义，不改变系统行为和 API 契约，仅补全遗漏的实现细节：

1. **I2C 互斥保护**（问题 1）：规格第 205-229 行已明确要求，实现时遗漏
2. **错误恢复流程**（问题 2）：规格第 237-246 行已明确要求，实现不完整
3. **MQTT 发布频率**（问题 3）：属于网络层实现细节，在设计文档 `openspec/changes/archive/2025-12-01-implement-air-quality-system/design.md:581-583` 中定义
4. **编码规范**（问题 4）：属于工程规范，在 `CLAUDE.md` 中定义

除第 1 和第 2 项涉及 `system-orchestration` 规格外，其他项不需要规格增量。

---

## 验证标准

所有场景必须通过以下验证：

1. **I2C 互斥访问**：
   - [ ] 运行 1 小时无 I2C 通信错误
   - [ ] OLED 显示无花屏
   - [ ] SHT35 数据无异常跳变

2. **传感器故障恢复**：
   - [ ] 物理断开 SHT35 后系统进入 `STATE_ERROR`
   - [ ] 重新连接后系统自动恢复到 `STATE_RUNNING`（约 5 分钟）
   - [ ] 决策任务在恢复后正常执行

3. **编译验证**：
   - [ ] `idf.py build` 无错误和警告

4. **规格符合性**：
   - [ ] 对照 `openspec/specs/system-orchestration/spec.md` 的所有场景都能通过
