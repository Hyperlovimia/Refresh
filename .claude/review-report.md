# 审查报告（2025-12-01 22:53 UTC+8, Codex）

## 结论
- 建议：**通过**（规格缺口已补齐，退化路径与驱动接口符合要求）
- 技术评分：88 / 100
- 战略评分：85 / 100
- 综合评分：87 / 100

## 评估结果
1. **CO₂ 缓存退化逻辑 ✅**  
   - `sensor_manager_read_all()` 现在在连续失败≥3次且有缓存时设置 `using_cache`，令 `data->co2` 取 `last_valid_co2`，并把 `data->valid` 定义为“温湿度正常即可使用”的语义，同时返回 `ESP_FAIL` 提示退化（main/sensors/sensor_manager.c:47-105）。  
   - `sensor_task()` 也改为只要 `data.valid` 就更新共享缓冲区，`ret == ESP_FAIL` 时打印缓存提示（main/main.c:210-227）。因而决策/网络模块在硬件断线时仍能获得上次有效的 CO₂ 值，满足 openspec “连续失败仍需提供上一次有效值” 场景。

2. **CO₂ 驱动规格符合性 ✅**  
   - UART RX 缓冲扩展至 1024 字节并在初始化时记录 `s_init_time`（main/sensors/co2_sensor.c:13-63），`co2_sensor_is_ready()` 根据 `xTaskGetTickCount()` 判断 300 秒预热窗口（main/sensors/co2_sensor.c:125-140）。  
   - `co2_sensor_calibrate()` 实现了 JX-CO2-102 的 MODBUS 校准命令并校验响应头（main/sensors/co2_sensor.c:143-177）。至此规格中关于缓冲区大小、预热判定与手动校准的场景均被覆盖。

## 测试与验证
- 未新增自动化测试；建议按用户计划执行 CO₂ 缓存传播、300 秒预热与现场校准验证，确保硬件层面无回归。

---

# 审查报告（2025-12-01 22:28 UTC+8, Codex）

## 结论
- 建议：**退回**（退化逻辑仍未让系统获得缓存数据，CO₂ 驱动接口也未按规格完成）
- 技术评分：65 / 100
- 战略评分：60 / 100
- 综合评分：63 / 100

## 评估结果
1. **CO₂ 缓存数据仍无法被系统使用（高）**  
   - `sensor_manager_read_all()` 虽然在失败 ≥3 次时写入 `data->co2 = last_valid_co2`，但 `data->valid` 依旧等于 `co2_valid && temp_valid && humi_valid`，当本次 CO₂ 读取失败时就被置为 false（main/sensors/sensor_manager.c:47-100）。  
   - `sensor_task()` 只会在 `ret == ESP_OK && data.valid` 条件成立时把数据写入共享缓冲区（main/main.c:215-224），所以缓存值从未传播到决策/网络等模块。规格场景“连续失败后使用上次有效值”（openspec/specs/sensor-integration/spec.md:121-128）依旧无法满足，硬件断开后系统完全没有任何 CO₂ 数据可用。

2. **CO₂ 驱动接口依旧不符合规格（高）**  
   - `co2_sensor_init()` 仍用 256 字节 RX 缓冲（main/sensors/co2_sensor.c:53-56），未达到规格要求的 1024 字节（openspec/specs/sensor-integration/spec.md:10-17）；在主动上报模式堆积多帧时仍有丢帧风险。  
   - `co2_sensor_is_ready()` 始终返回 true、`co2_sensor_calibrate()` 直接返回 `ESP_ERR_NOT_SUPPORTED`（main/sensors/co2_sensor.c:123-130），与预热判定/手动校准场景完全脱节（openspec/specs/sensor-integration/spec.md:36-53）。用户以“阶段三再优化”为由暂缓实现，但规范要求这些接口已经存在，相关调用方目前只能拿到硬编码结果。

## 测试与验证
- 未提供新的自动化或硬件测试记录；建议修复后至少重新运行 `idf.py build` 并对 CO₂ 断线场景做一次实机验证，确认缓存值真的能推送给上层。

---

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
