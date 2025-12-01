# Codex 分析上下文（2025-12-01 18:11 UTC+8, Codex）

## 接口契约
- `system_init()` 应负责在 `app_main()` 启动阶段创建全部同步原语并初始化各模块，规格要求包括 `data_mutex`、`i2c_mutex` 和 `system_events`（openspec/specs/system-orchestration/spec.md#L201-L230）。
- 网络任务需要同时执行天气拉取（10 分钟）和 MQTT 状态上报（30 秒/状态变化）两类工作，接口由 `mqtt_publish_status()`/`mqtt_publish_alert()` 暴露（openspec/changes/archive/2025-12-01-implement-air-quality-system/design.md#L565-L590）。

## 边界条件
- I2C 总线由 SHT35 读写和 OLED 刷屏共享，必须依赖互斥锁避免总线冲突；当前代码仅创建 `data_mutex/system_events/alert_queue`（main/main.c#L43-L46）且不存在任何 `i2c_mutex` 引用。
- 传感器故障恢复流程需要在错误状态解除后重新执行预热与稳定阶段，否则状态机会卡在 INIT，导致 `decision_task` 永久休眠（main/main.c#L137-L214）。
- MQTT 发布频率需相对于 30 秒节拍工作；`MQTT_PUBLISH_INTERVAL_SEC` 在主头文件中定义但未被使用（main/main.h#L95-L97 主动常量，`rg` 没有匹配）。

## 风险评估
- 缺少 I2C 互斥锁可能导致 OLED 和 SHT35 同时操作总线时出现驱动崩溃或阻塞，属于并发安全缺陷。
- 错误状态恢复逻辑无法重新运行 `system_init()` 或重置状态机，传感器恢复后系统仍停留在 INIT，造成业务长期不可用。
- MQTT 状态包只会在 10 分钟网络周期内发送一次，远超规格要求，可能触发后台监控误报或 SLA 违规。

## 技术建议
1. 在 `main/main.c` 定义 `static SemaphoreHandle_t i2c_mutex` 并于 `system_init()` 调用 `xSemaphoreCreateMutex()`；SHT35 采样与 OLED 刷屏前后调用 `xSemaphoreTake/Give(i2c_mutex)`。
2. 拆分网络任务的节拍：保持 10 分钟天气轮询，同时新增以 `MQTT_PUBLISH_INTERVAL_SEC` 为基准的循环发布，或改为两个 FreeRTOS 任务/软件定时器。
3. 在 `handle_error_state()` 检测传感器恢复后，调用新的 `system_recover()`：重新运行关键子模块初始化、清零计时器并显式 `state_transition(STATE_PREHEATING)`。

## 观察报告
- 传感器、网络、显示等多个 `.c` 文件中的中文注释和日志字符串出现大量乱码（如 main/sensors/co2_sensor.c#L1-L30、main/network/wifi_manager.c#L1-L27、main/ui/oled_display.c#L1-L34），表明编码未按“所有注释使用 UTF-8 中文”规范存储，需要重新保存或替换为有效文案。
- `network_task()` 仅依据 `WEATHER_FETCH_INTERVAL_SEC` 延迟（main/main.c#L250-L284），没有引用 `MQTT_PUBLISH_INTERVAL_SEC`，直接导致 MQTT 发布周期与规格不符。
