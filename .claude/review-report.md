# 审查报告 — 网络服务实现交付
日期：2025-12-03 15:24 (UTC+8)  
审查人：Codex

## 结论
- 综合评分：45/100  
- 建议：需讨论（存在多处与规格/文档不符的缺陷，需补齐功能后再确认验收）

## 技术维度评分
- 代码正确性：45 — WiFi 事件处理阻塞系统事件循环，天气数据字段缺失，MQTT 告警路径不完整。
- 需求符合度：40 — menuconfig WiFi 配置、SmartConfig 触发、告警发布等关键需求未落地。
- 测试与可验证性：50 — 有日志描述，但缺乏针对上述缺陷的验证或测试记录。

## 战略维度评分
- 需求匹配：45 — README/配置指南宣称的能力（手动配网、CO₂ 告警等）与实现不符。
- 风险识别：55 — 代码包含一定日志/降级逻辑，但对事件阻塞、功能缺失未做任何防护。

## 主要发现
1. **menuconfig WiFi 配置完全无效**  
   - 实现仅尝试从 NVS 读取凭据并提示“请调用 wifi_manager_start_provisioning()”，无任何 `CONFIG_WIFI_*` 读取逻辑（`main/network/wifi_manager.c:222-257`）。  
   - README 与配置指南却指导用户在 menuconfig 中填写 SSID/密码（`README.md:184-213`、`openspec/changes/implement-network-services/configuration-guide.md:248-273`），导致文档所述方式无法生效。

2. **SmartConfig 无触发路径**  
   - `wifi_manager_start_provisioning()` 只在 `main/network/wifi_manager.c:263-311` 定义，整个代码库没有任何调用点；系统启动后仅打印“请调用 … 进行配网”（`main/network/wifi_manager.c:253-257`），无法真正进入 SmartConfig 流程。

3. **WiFi 事件处理阻塞系统事件循环**  
   - 在 `WIFI_EVENT_STA_DISCONNECTED` 分支中直接调用 `vTaskDelay(pdMS_TO_TICKS(5000/10000))`（`main/network/wifi_manager.c:61-85`），此回调运行在 ESP 事件循环/驱动上下文，会暂停所有后续 WiFi/IP/SmartConfig 事件 5~10 秒，违反 ESP-IDF 不可阻塞事件处理器的约定。

4. **天气 API 未按规格解析温度/风速**  
   - 解析函数只读取 PM2.5，并写死温度 20℃ / 风速 0（`main/network/weather_api.c:88-103`），与任务要求“提取 `now->temp`、`now->windSpeed` 字段”不符（`openspec/changes/implement-network-services/tasks.md:61-88`），会导致决策算法始终使用伪造数据。

5. **MQTT 告警发布缺失（仅 UI 告警）**  
   - `sensor_task` 将 CO₂ 告警写入本地队列（`main/main.c:210-235`），`network_task` 仅周期性调用 `mqtt_publish_status()`（`main/main.c:327-342`），而 `mqtt_publish_alert()` 仅在传感器故障时调用（`main/main.c:164-177`）。  
   - README 明确宣称 CO₂>1500 ppm 会推送 `home/ventilation/alert`（`README.md:203-213`），但实际实现从未向 MQTT 告警主题发布空气质量告警。

## 残留风险与建议
- menuconfig/SmartConfig 功能需决定真实使用方式：若继续依赖 SmartConfig，应删除无效配置项并补充触发接口；若要支持手动配置，则必须读取 `CONFIG_WIFI_*` 并在缺省时写入 NVS。
- WiFi 事件重试逻辑应迁移到定时器或独立任务，避免阻塞 ESP 事件循环。
- 天气 API 应补齐温度/风速字段解析，或更新规格/决策逻辑以避免使用固定值。
- 需要一条明确的告警→MQTT 发布链路（可复用现有 `alert_queue`），并补充对应测试/日志。
