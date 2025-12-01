# Codex 分析上下文（2025-12-01 21:37 UTC+8, Codex）

## 接口契约
- `co2_sensor_read_ppm()` 仍然依赖 “`  xxxx ppm\r\n`” 帧并只校验 0-5000 ppm（main/sensors/co2_sensor.c:64-120），与增量规格要求的 `CO2:xxxx\r\n` 帧和 400-5000 ppm 范围（openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:12-35）不一致；若固件切换到 `CO2:` 帧或需要过滤 400ppm 以下读数将直接返回 -1。
- 传感器管理器没有缓存上一次有效数据，也不会在 CO₂ 连续失败 3 次时降级使用旧值（main/sensors/sensor_manager.c:34-88），无法满足 `sensor_manager_read_all()` “连续失败仍需提供上一次有效值” 的场景描述（openspec/specs/sensor-integration/spec.md:62-86）。

## 边界条件
- `co2_sensor_is_ready()` 仍是固定返回 true 的桩（main/sensors/co2_sensor.c:123-126），无法基于 60s 预热 / 300s 稳定窗口判断 readiness（openspec/specs/sensor-integration/spec.md:16-36）。
- `co2_sensor_calibrate()` 未实现 MODBUS 指令发送（main/sensors/co2_sensor.c:128-130），与手动校准情景的 ESP_OK 返回要求（openspec/specs/sensor-integration/spec.md:29-41）冲突。
- `sht35_read()` 内部并未获取/释放 I2C 互斥锁（main/sensors/sht35.c:71-134），而规格将互斥锁作为该函数步骤的一部分（openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:67-98），需要澄清职责划分或者在驱动层补齐。

## 风险评估
- SCL/PWM 引脚在代码中改为 GPIO20/GPIO26，但 `sht35.h`、`fan_control.h` 及 OpenSpec 仍记载 GPIO22/GPIO25（main/sensors/sht35.h:11-27, main/actuators/fan_control.h:14-55, openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:57-158），极易导致硬件接线或测试任务误连。
- UART 缓冲区仍配置 256 字节（main/sensors/co2_sensor.c:53-56），小于规格要求的 1024 字节（openspec/specs/sensor-integration/spec.md:8-21）；若后续叠加更多传感器帧可能覆盖已有数据。

## 技术建议
1. 解析逻辑可兼容 `CO2:xxxx` 与 `xxxx ppm` 两种帧格式，并将有效范围 clamp 到 400-5000 ppm，以同时满足手册与规格。
2. 在驱动层维护预热计时与 MODBUS 校准命令，避免所有上层模块都无法判断 readiness / 触发校准。
3. 传感器管理器应缓存最后一次有效的 `SensorData`，并在 CO₂ 连续失败阈值达到时返回旧值 + `ESP_FAIL`，同时向 UI/决策层抛出健康状态。
4. 尽快同步 SCL/PWM 引脚修改到 `.h` 文档与 OpenSpec，保持硬件与软件一致。

## 观察报告
- `fan_control.c` 已锁定 GPIO26/25kHz/8bit 配置，但头文件与规格仍旧说明 GPIO25，且测试清单也指向 GPIO25（main/actuators/fan_control.c:15-146, openspec/changes/implement-sensor-actuator-drivers/specs/sensor-integration/spec.md:101-178）；建议在下一个提案中统一。
- `main/main.h` 中 “CO?”、“μg/m?” 等乱码仍然存在，说明编码治理未完成，将影响后续第三阶段算法实现时的可读性。

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
# Codex 分析上下文（2025-12-01 22:28 UTC+8, Codex）

## 接口契约
- `sensor_manager_read_all()` 虽然新增了 `last_valid_co2` 缓存，但 `data->valid` 仍然直接等于 `co2_valid && temp_valid && humi_valid`（main/sensors/sensor_manager.c:47-100），且 `sensor_task()` 只有在 `ret == ESP_OK && data.valid` 时才更新共享缓冲区（main/main.c:221-223）。因此当连续失败 3 次并使用缓存值时，`data.valid` 依旧为 false、返回值是 `ESP_FAIL`，上层根本拿不到缓存数据，无法满足“继续推送上次有效数据”的场景（openspec/specs/sensor-integration/spec.md:121-128）。
- `co2_sensor_init()` 仍只分配 256 字节 RX 缓冲（main/sensors/co2_sensor.c:53-56），与规格中“分配 1024 字节”要求冲突（openspec/specs/sensor-integration/spec.md:10-17）。

## 边界条件
- `co2_sensor_is_ready()` / `co2_sensor_calibrate()` 依旧是桩实现（main/sensors/co2_sensor.c:123-130），无法按规格提供预热就绪判定或 MODBUS 校准（openspec/specs/sensor-integration/spec.md:36-53）。阶段二状态机目前靠固定 60s/300s 计时运行，但只要其他模块调用这些接口就会拿到错误结果。

## 风险评估
- 传感器缓存值未能写入 `shared_sensor_data`，决策任务依然看不到 CO₂ 读数，一旦真实传感器断开系统会一直停留在旧数据（甚至 0）。结合错误恢复逻辑，容易导致系统长时间没有有效 CO₂ 数据却无法发出新告警。

## 技术建议
1. 在退化路径中引入 `bool co2_usable = co2_valid || (failure_count >= 3 && has_valid_cache)`，据此设置 `data->valid` 并允许 `sensor_task()` 在返回 `ESP_FAIL` 时继续写入共享缓冲区（可通过额外标志区分“缓存数据”与“实时数据”）。
2. 将 UART RX 缓冲区扩展为 1024 字节并实现 `co2_sensor_is_ready()` / `co2_sensor_calibrate()`，避免接口继续返回硬编码值。

# Codex 分析上下文（2025-12-01 21:37 UTC+8, Codex）
# Codex 分析上下文（2025-12-01 22:53 UTC+8, Codex）

## 接口契约
- `sensor_manager_read_all()` 现在通过 `using_cache` 标记确保 CO₂ 连续失败 ≥3 次时仍返回缓存值，并在温湿度有效时令 `data->valid` 为 true ，同时用返回值 `ESP_FAIL` 提示退化状态（main/sensors/sensor_manager.c:47-105）；`sensor_task()` 也改成只要 `data.valid` 就写入共享缓冲区，即使 `ret == ESP_FAIL`（main/main.c:210-227），满足规格“缓存值必须继续提供给系统使用”的场景（openspec/specs/sensor-integration/spec.md:121-128）。
- `co2_sensor_init()` 将 UART RX 缓冲扩展到 1024 字节，并在初始化完记录 `s_init_time`（main/sensors/co2_sensor.c:13-63），`co2_sensor_is_ready()` 基于 `xTaskGetTickCount()` 计算初始化后经过的秒数，未满 300 秒一律返回 false（main/sensors/co2_sensor.c:125-140），满足预热窗口判定。
- `co2_sensor_calibrate()` 已实现 JX-CO2-102 的 MODBUS 指令发送与响应校验（main/sensors/co2_sensor.c:143-177），符合集成规格的校准场景。

## 边界条件
- 传感器退化时 `data->valid` 依赖温湿度范围，因此若 I2C 数据正常即可保持 UI/决策可用；同时函数返回 `ESP_FAIL`，外部可以据此区分缓存状态（main/sensors/sensor_manager.c:98-105）。

## 风险评估
- `co2_sensor_is_ready()` 基于初始化时间戳，只要驱动没有重新安装就会沿用首次启动的计数；若未来需要在重新上电后重新计时，需要在 `co2_sensor_reinit()` 中显式复位 `s_uart_ready` 或提供单独的时间复位接口。
