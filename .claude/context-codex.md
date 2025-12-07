# 网络服务提案审查上下文
日期：2025-12-03 11:55 (UTC+8) — Codex

## 接口契约
- **wifi_manager_init**（`main/network/wifi_manager.h:17`，`openspec/changes/implement-network-services/specs/network-services/spec.md:11-59`）  
  输入：无；输出：`esp_err_t`。期望行为：初始化 STA、读取 NVS、若已有凭据自动连接，在连接失败场景中按规范等待 5 秒、重试 3 次之后返回 `ESP_FAIL`。事件处理需维护连接状态以供 `wifi_manager_is_connected()` 查询。
- **wifi_manager_start_provisioning**（`wifi_manager.h:25`，spec:21-31）  
  输入：无；输出：`esp_err_t`。需进入 SmartConfig、在 60 秒内获取凭据，成功后保存到 NVS 并连接。
- **weather_api_init/weather_api_fetch/weather_api_get_cached/weather_api_is_cache_stale**（`weather_api.h:17-44`，spec:78-142）  
  `weather_api_fetch` 需要在 WiFi 可用时发起 HTTPS GET，解析 `now.pm2p5/temp/windSpeed` 字段，填充 `WeatherData`，缓存有效期 30 分钟，超时返回 `ESP_FAIL` 并保持缓存不变。
- **mqtt_client_init/mqtt_publish_status/mqtt_publish_alert**（`mqtt_client.h:18-43`，spec:145-215）  
  初始化时必须创建 TLS 客户端、设置遗嘱并订阅 `home/ventilation/command`；`mqtt_publish_status` 需在 MQTT 连接可用时构建含 `co2/temp/humi/fan_state/mode/timestamp` 的 JSON 并发布 QoS0；`mqtt_publish_alert` 在告警时构建 JSON、QoS1 发布。

## 边界条件
- **WiFi**：首次启动 NVS 中没有凭据（spec:21-31）；连接失败（spec:41-49）；运行中断线需无限重连（spec:51-59）；`wifi_manager_is_connected()` 必须即时反映状态（spec:60-69）。
- **天气 API**：网络不可用或 API 失败时 `weather_api_fetch` 超时 10 秒并返回失败（spec:115-123）；缓存小于 30 分钟视为有效（spec:124-141）。
- **MQTT**：初始化必须可在 TLS 错误/凭据缺失时返回 `ESP_FAIL`（spec:151-161）；断线后 30 秒自动重连并重新订阅（spec:198-207）；WiFi 未连接时发布函数立即返回失败（spec:208-215）。

## 风险评估
1. **WiFi 重试语义冲突**：设计（`design.md:57-60`）要求 10 秒间隔无限重试，与规范（spec:41-49）“5 秒、最多 3 次后返回失败”的行为不一致，会导致 `system_init` 难以根据返回值判断结果。
2. **MQTT 状态缺失系统模式信息**：接口仅接收 `SensorData` 与 `FanState`（`mqtt_client.h:20-35`），任务清单未提到扩展接口，无法满足规范示例中 `mode` 字段（spec:163-178）的输出要求。
3. **MQTT 断线重连策略可能阻塞**：任务 3.5（`tasks.md:125-129`）计划在事件回调里等待 30 秒再执行 `esp_mqtt_client_reconnect`，会阻塞 MQTT 事件循环并延迟其它消息处理。

## 技术建议
- 对齐 WiFi 失败重试策略：明确 `wifi_manager_init` 是否应在 3 次失败后返回 `ESP_FAIL`（符合 spec）或改 spec 支持后台无限重试。
- 调整 MQTT 状态发布接口，增加 `SystemMode` 参数或提供可查询函数，以便构建含 `mode` 字段的 payload。
- 重连延迟应由定时器或独立任务处理，勿在 MQTT 事件回调中阻塞；可在回调里投递事件给任务或使用 `esp_timer`.
- 在配置章节明确 `Kconfig.projbuild` 需要新增 WiFi/API/MQTT 选项，并在 `idf_component_register` 里声明 `REQUIRES esp_wifi esp_netif esp_http_client esp_mqtt esp_crt_bundle`.

## 观察报告
- `main/main.c:440-474` 依赖 `wifi_manager_init/weather_api_init/mqtt_client_init` 的返回值做日志提示，一旦 `wifi_manager_init` 改为永不返回失败就失去诊断价值。
- 设计文档在非目标中声明“不实现 MQTT 订阅”（`design.md:33-35`），但规范（spec:151-161）要求初始化阶段即订阅 `home/ventilation/command`，需要澄清是否真正实现。

# 室内污染物扩展审查上下文
日期：2025-12-06 21:17 (UTC+8) — Codex

## 主要代码触点
- `main/main.h:60-110` 新增 `IndoorPollutants` 结构体并将 `SensorData` 的 CO₂ 字段替换为 `pollutants`，同时加入 PM/VOC/HCHO 合法范围常量。
- `main/sensors/sensor_manager.c:17-168` 引入 `manual_pollutants` 缓存，在 `sensor_manager_read_all()` 中填充 `data.pollutants.*`，并提供 `sensor_manager_set_pollutant()` 手动注入接口。
- `main/algorithm/decision_engine.c:23-35`、`main/main.c:223-236`、`main/network/mqtt_wrapper.c:233-242` 将所有 `sensor->co2` 访问改为 `sensor->pollutants.co2`。
- `main/ui/oled_display.h:29-42` / `main/ui/oled_display.c:18-49` 将 `oled_add_history_point` 签名改为接收 `SensorData *`，内部读取 `sensor->pollutants.co2`。

## 相关规范
- `openspec/specs/user-interface/spec.md:144-166` 仍规定 `oled_add_history_point(float co2)` 并在场景中以浮点参数示例，尚未反映指针签名。
- `openspec/specs/system-orchestration/spec.md:253-287` 要求 `sensor_manager_reinit()` “清空缓冲区和计数器”，用于 ERROR→INIT 恢复路径。
- `openspec/changes/extend-indoor-pollutant-types/specs/sensor-integration/spec.md` 描述 `SensorData.pollutants.*` 数据结构以及手动注入场景，但 UI spec 未包含在变更范围。

## 开放问题 / 风险
1. UI 能力规范仍以旧函数签名示例，代码更改未在 `specs/user-interface` 追加 delta，可能导致 API 合同不一致。
2. 新增的 PM/VOC/HCHO 合法范围常量目前未在 `sensor_manager_set_pollutant()` 或 `sensor_manager_read_all()` 中用于判定/夹取，`SensorData.valid` 仍只依赖 CO₂/温湿度。
3. `sensor_manager_reinit()`（`main/sensors/sensor_manager.c:124-147`）仅重置 CO₂ 缓存与传感器，但未清空 `manual_pollutants`，可能与 spec “清空缓冲区” 要求不符，导致错误恢复后仍输出旧的手动数值。

# remove-local-online-decision 审查上下文
日期：2025-12-06 23:37 (UTC+8) — Codex

## 接口契约
- **decision_make/decision_detect_mode**（`main/algorithm/decision_engine.c:12-38`）需符合 spec delta 对 MODE_REMOTE/LCOAL 的定义，尤其是“未收到有效远程命令则返回上一次有效风扇状态”的场景（`openspec/changes/remove-local-online-decision/specs/decision-algorithm/spec.md:21-74`）。
- **mqtt_publish_status**（`main/network/mqtt_wrapper.c:260-310`）必须遵守“MQTT 状态发布仅在 `sensor->valid == true` 时执行”的新契约，并在数据无效时记录跳过日志（`openspec/changes/remove-local-online-decision/specs/network-services/spec.md:98-114`）。
- **mqtt_get_remote_command`/命令订阅**（`main/network/mqtt_wrapper.c:96-369`）应覆盖 spec 中关于命令接收、缓冲与错误处理的场景，包括“无有效命令 -> 返回 false 且不修改输出”（`openspec/changes/remove-local-online-decision/specs/network-services/spec.md:45-95`）。

## 关键疑问
1. MODE_REMOTE 时 `decision_task` 忽略 `mqtt_get_remote_command()` 的返回值（`main/main.c:274-279`），在 WiFi 刚恢复但远程命令尚未抵达时会直接把风扇设为 `remote_cmd` 的默认值（`FAN_OFF`），与 spec 场景“未收到有效命令时保持上一有效状态”冲突。
2. MQTT 状态发布链路（`main/main.c:317-330` -> `main/network/mqtt_wrapper.c:260-310`）没有检查 `sensor->valid`，系统预热/稳定阶段产生的无效数据仍会发送到服务器，不满足“确保传感器发送的数据为正常运行状态”的业务描述。
3. menuconfig 与 README 仍要求填写和风天气 API Key（`main/Kconfig.projbuild:19-35`、`README.md:142-210`），但天气模块已删除，是否需要清理配置项/文档以免误导？

## 风险提示
- 若远程命令缺失即退回 `FAN_OFF`，一旦 MQTT 链路异常就会导致风扇强制停机，无法满足“远程命令缺失 → 维持上一有效状态”的契约。
- 无效的传感器快照被推送至 MQTT，远程服务器据此决策会产生噪声甚至误判，特别是在系统刚进入 STATE_RUNNING 的阶段。
- 未清理的天气配置/文档会让部署者继续申请 API Key 并在 menuconfig 中填入“必需”字段，却完全无处使用，交付体验受损。

# expand-multi-fan-control 审查上下文
日期：2025-12-07 17:50 (UTC+8) — Codex

## 核心代码触点
- `main/main.h:27-70` 定义 `FAN_COUNT=3`、`FanId` 与 `MultiFanState` 结构，为多风扇模块提供一致的枚举/数组维度。
- `main/actuators/fan_control.c:8-200` 将 GPIO 映射与 LEDC 通道改为数组（Fan0→GPIO26, Fan1→GPIO27, Fan2→GPIO33），`fan_control_set_state(id, state)`、`fan_control_set_all()` 负责独立/批量控制。
- `main/algorithm/decision_engine.c:12-54` 的 `decision_make()` 现以 `FanState[FAN_COUNT]` 作为输入/输出：MODE_REMOTE 直接使用远程数组、MODE_LOCAL 同步 3 个风扇、MODE_SAFE_STOP 全部关闭。
- `main/network/mqtt_wrapper.c:32-310` 解析命令 `fan_0`~`fan_2` 并在状态 JSON 中输出相同字段；`mqtt_get_remote_command()`/`mqtt_publish_status()` 的签名均接受 `FanState` 数组。
- `main/main.c:39-383` 新建 `shared_fan_states[FAN_COUNT]`，决策任务更新数组，网络/显示任务在发布与 UI 中读取。

## 相关规范
- `openspec/changes/expand-multi-fan-control/specs/actuator-control/spec.md:1-120` 规定三风扇的 GPIO/LEDC 映射、批量控制和查询场景。
- `openspec/changes/expand-multi-fan-control/specs/network-services/spec.md:1-118` 描述命令/状态 JSON 均需包含 `fan_0/fan_1/fan_2`，并支持部分更新。

## 关键疑问
1. `handle_error_state()` 仅调用 `fan_control_set_all(FAN_OFF)`（`main/main.c:162-175`），但未写回 `shared_fan_states`。`network_task` 和 `display_task` 仍读取旧数组（`main/main.c:335-343`, `375-383`），安全停机后 MQTT/UI 会继续显示过时状态，是否违背网络服务 spec 的“状态上报反映真实硬件”要求？
2. 官方指南 `Guides/MQTT.md` 仍展示单字段 `fan_state`（`Guides/MQTT.md:24-120, 262-299`）及 `shared_fan_state` 数据流，而实现只解析 `fan_0`~`fan_2`（`main/network/mqtt_wrapper.c:93-111`）。若文档不更新，远程控制端会依据旧格式发送命令并被忽略，是否需要提供兼容层或同步更新指南？
