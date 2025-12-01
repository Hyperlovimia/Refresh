# 审查报告（2025-12-01 21:37 UTC+8, Codex）

## 结论
- 建议：**退回**（核心驱动接口与规格不符，且健康管理逻辑缺失）
- 技术评分：60 / 100
- 战略评分：55 / 100
- 综合评分：58 / 100

## 评估结果
1. **CO₂ 帧解析与范围验证未按规格实现（高）**  
   - 规格要求解析 `CO2:xxxx\r\n` 帧并限制在 400-5000 ppm（openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:12-35）。当前实现依赖 “`  xxxx ppm`” 子串并仅检查 0-5000（main/sensors/co2_sensor.c:64-118），当固件或上位机输出 `CO2:850` 时无法找到 `ppm` 字样直接判定失败，同时 <400 ppm 的低值会被错误当成有效读数。  
   - 影响：UART 帧格式一旦按规格部署将完全读不到 CO₂，且会让 200-399 ppm 这类异常值进入决策链条。

2. **CO₂ 驱动接口仍是桩实现（高）**  
   - `co2_sensor_is_ready()` 始终返回 true，`co2_sensor_calibrate()` 直接报不支持，两者都与规格“基于 60s/300s 窗口判定就绪”及“发送 MODBUS 校准命令”的场景冲突（main/sensors/co2_sensor.c:123-130, openspec/specs/sensor-integration/spec.md:16-41）。  
   - 同时 `co2_sensor_init()` 仍只分配 256 字节 RX 缓冲（main/sensors/co2_sensor.c:53-56），而规格要求 1024 字节以容纳多帧（openspec/specs/sensor-integration/spec.md:8-21）。这些缺口会导致上电状态机永远视为 ready、无法执行外部校准，并有丢帧风险。

3. **连续失败退化逻辑缺失（中）**  
   - 规格要求 `sensor_manager_read_all()` 在 CO₂ 连续失败 3 次后使用上一次有效值并返回 `ESP_FAIL`，同时 `sensor_manager_is_healthy()` 应基于该计数判断（openspec/specs/sensor-integration/spec.md:62-104）。当前实现既不缓存上一次成功数据，也不会在 CO₂ 读取失败时累计计数（main/sensors/sensor_manager.c:34-88），所以无法满足“可回放旧值并上报健康状态”的要求。

4. **SHT35/I2C 锁职责与规格不符（中）**  
   - SHT35 读取场景明确第一步就是“获取 I2C 互斥锁”（openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:67-98），但 `sht35_read()` 并未涉及互斥，完全依赖调用者（main/sensors/sht35.c:71-134）。若其他调用者未记得拿锁，就会违反规格并重新引入总线竞争。至少需要在文档中声明职责变化或直接在驱动层加锁。

5. **GPIO 引脚文档未同步（低）**  
   - 代码已改为 SDA=GPIO21/SCL=GPIO20、PWM=GPIO26（main/sensors/sht35.c:13-47, main/actuators/fan_control.c:15-146），但 `sht35.h`、`fan_control.h` 以及 OpenSpec/测试任务仍指向 GPIO22 / GPIO25（main/sensors/sht35.h:11-27, main/actuators/fan_control.h:14-55, openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:57-158）。这种文档漂移会让硬件接线和测试按旧引脚执行，导致现场调试失败。

## 测试与验证
- 未执行额外自动化测试；建议在修复上述问题后至少重新运行 `idf.py build` 并完成 UART/I2C/LEDC 烟雾测试。

---

# 审查报告（2025-12-01 20:12 UTC+8, Codex）

## 结论
- 建议：**需讨论**（核心功能缺陷已补齐，但编码规范仍未满足）
- 技术评分：78 / 100
- 战略评分：70 / 100
- 综合评分：74 / 100

## 评估结果
1. **I2C 互斥保护 ✅ 已解决**  
   - `system_init()` 现在创建 `i2c_mutex` 并暴露 `get_i2c_mutex()`，SHT35 与 OLED 都在锁保护下访问总线（main/main.c:43-main/main.c:64, main/main.c:407-main/main.c:417, main/sensors/sensor_manager.c:47-main/sensors/sensor_manager.c:63, main/ui/oled_display.c:18-main/ui/oled_display.c:44）。  
   - 满足 openspec 中“创建互斥锁 + I2C 串行访问”情景，消除了原有竞争风险。

2. **错误恢复流程 ✅ 已解决**  
   - `handle_error_state()` 在检测传感器恢复后调用 `sensor_manager_reinit()` 并重新进入 `STATE_INIT`（main/main.c:164-main/main.c:199, main/sensors/sensor_manager.c:90-main/sensors/sensor_manager.c:112）。  
   - `handle_init_state()` 区分首次启动与恢复场景，能再次触发 PREHEATING/STABILIZING 时序，符合规格“恢复后重新初始化并重新进入运行态”的要求。

3. **MQTT 发布频率 ✅ 已解决**  
   - `network_task()` 拆分天气和 MQTT 两个节拍，新增 `last_mqtt_publish` 并以 `MQTT_PUBLISH_INTERVAL_SEC`=30 为周期发布状态（main/main.c:288-main/main.c:345, main/main.h:95-main/main.h:97）。  
   - 30 秒节拍 + 状态变化逻辑具备实施基础，符合设计文档 7.3 节的频率要求。

4. **中文注释乱码 ⚠️ 仍未完全修复（低）**  
   - 至少 `main/sensors/co2_sensor.c:1-main/sensors/co2_sensor.c:76`、`main/algorithm/decision_engine.c:1-main/algorithm/decision_engine.c:59`、`main/algorithm/local_mode.c:1-main/algorithm/local_mode.c:22` 仍出现 “??” 字符。  
   - 这些模块是后续阶段要实现的核心驱动/算法，继续保持乱码会让硬件引脚、算法意图无法追踪，违背项目“所有注释与日志使用 UTF-8 中文”的要求。  
   - 建议统一替换为可读中文描述后再继续归档，确保后续实现者拥有完整上下文。

## 测试与验证
- 未执行新的自动化测试；此轮复核基于代码审阅与规格对照。若后续再调整，建议重新运行 `idf.py build` 确认无回归。
