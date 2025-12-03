# 审查报告 — implement-network-services 提案
日期：2025-12-03 12:00 (UTC+8)  
审查人：Codex

## 结论
- 综合评分：62/100  
- 建议：需讨论（存在跨文档冲突与实现风险，需在审批前澄清）

## 技术维度评分
- 设计完整性：65 — 架构路径清晰，但 WiFi/MQTT 细节与规范冲突。
- 可实现性：60 — 任务拆分细致，却遗漏 `SystemMode` 传递、TLS 证书配置等关键点。
- 测试计划：55 — 提及集成测试，但未覆盖规范强调的失败场景（WiFi 重试上限、MQTT 断线处理边界）。

## 战略维度评分
- 需求匹配：60 — 覆盖 WiFi/天气/MQTT 的核心需求，但 payload 字段、命令订阅等与现有系统/规格不一致。
- 风险识别：70 — 文件列出了主要风险，不过漏掉接口契约冲突。

## 主要发现
1. **WiFi 失败重试语义与规范冲突**  
   - 规范 `openspec/changes/implement-network-services/specs/network-services/spec.md:41-49` 明确 `wifi_manager_init()` 在连接失败时需等待 5 秒、重试 3 次，三次失败后返回 `ESP_FAIL`。  
   - 设计 `design.md:57-60`、成功标准 `proposal.md:124-125` 改为 10 秒间隔无限重试，并未说明何时返回失败。  
   - `main/main.c:440-451` 依赖 `wifi_manager_init()` 的返回值打印警告；若永不返回失败，系统无法得知初始化异常。需重新对齐重试策略或同步更新规范。

2. **MQTT 状态消息缺少 `mode` 数据路径**  
   - 规范示例及接口注释要求 payload 含 `mode` 字段（`spec.md:163-178`、`main/network/mqtt_client.h:20-35`），但 `mqtt_publish_status()` 仅接收 `SensorData` 与 `FanState`，调用点 `main/main.c:328-347` 也没有系统模式参数。  
   - 设计/任务并未要求扩展 API 或提供 `SystemMode` 访问方法，因此无法生成符合规范的 JSON。需在提案中明确如何传递运行模式。

3. **MQTT 断线重连计划会阻塞事件回调**  
   - 任务 3.5（`openspec/changes/implement-network-services/tasks.md:125-129`）打算在 `mqtt_event_handler()` 里“等待 30 秒后自动重连”。  
   - MQTT 事件回调运行于 esp-mqtt 任务上下文，阻塞 30 秒会卡住整条 MQTT 状态机，导致其它事件（如 PUBLISH ack）无法处理。应改为在回调里投递工作给独立任务或定时器。

4. **设计声明“不实现 MQTT 订阅”但规范仍要求订阅命令主题**  
   - 设计明确非目标为“不实现 MQTT 订阅和远程控制”（`design.md:33-35`），然而规范场景 `spec.md:151-161` 仍要求在初始化时订阅 `home/ventilation/command`。  
   - 若确实暂不实现订阅，则需要更新规范/Success Criteria；若要满足规范，则需在任务和实施计划中补充订阅及消息处理逻辑。

## 残留问题 / 建议讨论
- WiFi 重试行为需决策：保持规范的 3 次同步重试，还是明确 `wifi_manager_init()` 立即返回并在后台无限重试？
- MQTT 状态 payload 的 `mode` 应如何获取？是否需要拓展 API 或在 `mqtt_publish_status` 内部读取新的全局状态接口？
- MQTT 命令订阅是否属于此次交付范围？若不是，需在 spec delta 中调整 MODIFIED 场景。

## 待验证事项
- `esp_crt_bundle_attach` 依赖的 `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE*` 选项是否已在配置计划中体现。
- SmartConfig 触发流程：系统如何在缺省凭据时触发 `wifi_manager_start_provisioning()`，是否需要按键/命令支持。
