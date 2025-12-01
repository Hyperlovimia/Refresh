# 审查报告（2025-12-01 18:12 UTC+8, Codex）

## 结论
- 建议：**退回**（核心并发与恢复逻辑缺失，无法满足规格）
- 技术评分：45 / 100
- 战略评分：40 / 100
- 综合评分：43 / 100

## 发现
1. **缺少规格要求的 I2C 互斥锁，存在总线竞争风险（严重）**  
   - 规格明确要求在初始化阶段创建 `i2c_mutex` 并在 SHT35/OLED 访问时加锁以避免 I2C 冲突（openspec/specs/system-orchestration/spec.md#L205-L229）。  
   - 代码仅声明 `data_mutex/system_events/alert_queue`，既未定义也未创建 `i2c_mutex`（main/main.c:43-main/main.c:46, main/main.c:347-main/main.c:350），SHT35 读取和 OLED 渲染也没有锁保护。  
   - 结果：显示任务和传感器任务可以同时驱动 I2C，总线可能挂死或返回错误数据。

2. **传感器故障恢复后不会重新初始化，状态机卡在 INIT（严重）**  
   - 规格要求故障后“每 10 秒重试一次，恢复后重新初始化”并重新进入正常流程（openspec/specs/system-orchestration/spec.md#L237-L246, openspec/changes/archive/2025-12-01-implement-air-quality-system/design.md#L631-L639）。  
   - `handle_error_state()` 在检测到恢复时只调用 `state_transition(STATE_INIT)`（main/main.c:137-main/main.c:162），而 `handle_init_state()` 不含任何逻辑，`system_init()` 也只在 `app_main()` 中调用一次（main/main.c:417-main/main.c:469）。  
   - 因此故障恢复后不会重新进入 PREHEATING/STABILIZING，核心任务依赖的 `current_state >= STATE_RUNNING` 条件一直不满足，系统长时间停滞。

3. **MQTT 状态上报频率不符规格（中）**  
   - 设计规定状态消息需每 30 秒或状态变化时上报（openspec/changes/archive/2025-12-01-implement-air-quality-system/design.md#L565-L585），并在头文件中定义了 `MQTT_PUBLISH_INTERVAL_SEC`=30（main/main.h:95-main/main.h:97）。  
   - `network_task()` 仅按照 `WEATHER_FETCH_INTERVAL_SEC`（600 秒）循环执行，并在同一循环中调用 `mqtt_publish_status()`（main/main.c:250-main/main.c:284）；`MQTT_PUBLISH_INTERVAL_SEC` 完全未被使用（`rg` 仅在 main/main.h 中命中）。  
   - 结果：MQTT 状态只会每 10 分钟发送一次，违反规格的 30 秒节拍要求。

4. **多处注释与日志字符串为乱码，违背“中文注释”要求（低）**  
   - 如 `main/sensors/co2_sensor.c:1-main/sensors/co2_sensor.c:30`、`main/network/wifi_manager.c:1-main/network/wifi_manager.c:27`、`main/ui/oled_display.c:1-main/ui/oled_display.c:34` 均显示 “???” 字符。  
   - 这些文件原本应说明硬件引脚、日志内容及 TODO，但因为编码错误，开发者无法理解预期操作，后续阶段无法参照这些接口。

## 测试与验证
- 未执行自动化测试；本次审查通过代码阅读与规格对照完成。建议修复后至少运行 `idf.py build` 验证。
