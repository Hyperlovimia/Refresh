# Codex 分析上下文（2025-12-01 20:11 UTC+8, Codex）

## 接口契约
- `system_init()` 现已在启动阶段创建 `data_mutex`、`i2c_mutex`、`system_events` 等同步对象，并通过 `get_i2c_mutex()` 向其他模块暴露 I2C 锁（main/main.c:43-main/main.c:64, main/main.c:407-main/main.c:417）。
- `network_task()` 使用两个独立计数器管理 10 分钟天气轮询与 30 秒 MQTT 发布（main/main.c:288-main/main.c:345），满足规格要求的双节拍。

## 边界条件
- 传感器错误恢复路径：`handle_error_state()` 每 10 秒检查健康状态，成功后调用 `sensor_manager_reinit()` 并回到 `STATE_INIT`（main/main.c:164-main/main.c:199, main/sensors/sensor_manager.c:90-main/sensors/sensor_manager.c:112）。`handle_init_state()` 利用状态标志重新进入 PREHEATING，从而再次执行 60 秒预热和 240 秒稳定阶段（main/main.c:82-main/main.c:125）。
- SHT35 读写与 OLED 刷屏都围绕同一个 I2C 锁执行，超时 100ms 以防任务被永久阻塞（main/sensors/sensor_manager.c:47-main/sensors/sensor_manager.c:63, main/ui/oled_display.c:18-main/ui/oled_display.c:44）。
- 源代码仍存在多处中文注释和日志乱码（例如 main/sensors/co2_sensor.c:1-main/sensors/co2_sensor.c:76、main/algorithm/decision_engine.c:1-main/algorithm/decision_engine.c:59、main/algorithm/local_mode.c:1-main/algorithm/local_mode.c:22），与“全部注释使用 UTF-8 中文”规范冲突。

## 风险评估
- I2C 互斥锁已经落实，硬件总线冲突风险显著降低。
- 错误恢复路径能够重新执行预热/稳定阶段，但 `handle_init_state()` 对事件位的依赖较脆弱，如未来清除 EVENT_SENSOR_READY 可能导致恢复流程失效，需要在后续迭代中关注。
- 编码问题尚未解决，导致驱动和算法模块的注释与日志不可读，阻碍后续实现与调试。

## 技术建议
1. 将 `handle_init_state()` 的判定逻辑改为显式标记而非依赖事件位，以免未来在清除事件位时破坏恢复路径。
2. 清理剩余乱码文件（至少包含 `co2_sensor.c`、`decision_engine.c`、`local_mode.c`），统一改写为可读中文注释与日志。

## 观察报告
- I2C 锁在 `sensor_manager_read_all()` 和 `oled_display_*` 中被调用，证实互斥保护已经补齐。
- MQTT 发布频率修正为 `MQTT_PUBLISH_INTERVAL_SEC` 节拍，但 `wifi_manager_is_connected()` 仍是桩函数，后续需要结合真实连接状态测试。
- 多个模块（co2_sensor/decision_engine/local_mode）仍输出 “??” 字符，说明阶段 4 的编码修复并未覆盖全部文件，仍需整改。
