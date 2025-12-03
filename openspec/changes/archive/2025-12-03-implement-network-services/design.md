# 设计文档：网络服务模块实现

## Context

本变更为智能室内空气质量改善系统实现完整的网络服务模块，包括 WiFi 管理、天气 API 客户端和 MQTT 客户端。

### 背景
- 当前代码仅有桩函数，无法提供实际网络功能
- 系统依赖网络服务获取外部数据（天气 API）和上报状态（MQTT）
- 需要支持网络降级模式（缓存数据）和本地自主决策（离线）

### 约束条件
- 硬件：ESP32-S3，内存有限（~320KB SRAM）
- 网络：WiFi 2.4GHz，可能不稳定
- 外部服务：和风天气 API（HTTPS），EMQX Cloud（MQTT over TLS）
- 开发框架：ESP-IDF 5.5.1

### 利益相关者
- 系统主逻辑（main.c）：依赖网络服务提供数据和连接状态
- 决策算法（decision_engine.c）：需要天气数据进行决策
- 用户：期望系统能自动连接 WiFi，在网络故障时仍能工作

## Goals / Non-Goals

### Goals
- 实现健壮的 WiFi 连接管理（自动连接、断线重连）
- 实现可靠的天气数据获取和缓存（30 分钟有效期）
- 实现 MQTT 状态上报和告警发布（QoS 0/1）
- 支持首次配网（SmartConfig）
- 所有网络操作提供清晰的错误处理和日志
- 内存占用合理（HTTP/MQTT 缓冲区优化）

### Non-Goals
- ❌ 不实现 MQTT 订阅和远程控制（预留接口，后续扩展）
- ❌ 不实现 BLE 配网（仅 SmartConfig）
- ❌ 不实现 OTA 固件升级
- ❌ 不实现本地 Web 服务器配置界面
- ❌ 不实现多 AP 漫游或负载均衡

## Decisions

### 1. WiFi 管理策略

**决策**：使用 ESP-IDF 标准 WiFi STA 模式 + NVS 存储 + SmartConfig 配网

**理由**：
- ESP-IDF 提供成熟的 WiFi 驱动和事件系统，无需自研
- NVS 非易失存储适合保存 WiFi 凭据（掉电不丢失）
- SmartConfig 是最简单的配网方式（无需物理按键或屏幕输入）

**替代方案考虑**：
- ❌ **BLE 配网**：需要额外的 BLE 代码和手机 APP，复杂度高
- ❌ **AP 模式配网**：需要启动 HTTP 服务器，内存占用大
- ❌ **WPS**：安全性低，部分路由器不支持

**实现细节**：
- WiFi 初始化时先检查 NVS 是否有保存的配置
- 如有配置，直接连接；如无配置，提示用户调用 `wifi_manager_start_provisioning()`
- **初始化阶段重试策略**：连接失败时等待 5 秒，最多重试 3 次，失败后返回 `ESP_FAIL`（让主逻辑判断是否需要降级）
- **运行时重连策略**：连接断开后在事件处理器中每 10 秒自动重连，无限重试（避免短暂网络抖动导致系统失效）
- 使用 FreeRTOS EventGroup 标记连接状态，供其他模块查询

### 2. 天气 API 客户端设计

**决策**：使用 esp_http_client + cJSON + 静态缓存

**理由**：
- esp_http_client 是 ESP-IDF 内置组件，支持 HTTPS 和 TLS
- cJSON 是 ESP-IDF 内置组件，轻量级且易用
- 静态缓存（全局变量）避免动态内存分配，减少碎片化风险

**替代方案考虑**：
- ❌ **自研 HTTP 客户端**：工作量大，稳定性差，不推荐
- ❌ **使用 ESP-IDF HTTP Server**：这是服务器端组件，不适用
- ❌ **使用第三方 JSON 库（如 ArduinoJson）**：引入额外依赖，cJSON 已足够

**实现细节**：
- API URL：`https://api.qweather.com/v7/air/now?location={城市代码}&key={API_KEY}`
- 响应格式：
  ```json
  {
    "now": {
      "pm2p5": "35",
      "temp": "22",
      "windSpeed": "12"
    }
  }
  ```
- 缓存策略：
  - 成功获取后保存到静态变量 `cached_data`
  - 记录时间戳 `cached_data.timestamp`
  - 检查时间差 > 30 分钟则视为过期
  - 网络不可用时优先使用缓存（即使过期）
- 超时设置：10 秒（避免长时间阻塞）
- 错误处理：HTTP 状态码非 200 或 JSON 解析失败时返回 ESP_FAIL

### 3. MQTT 客户端设计

**决策**：使用 esp_mqtt + TLS + QoS 0/1 + 遗嘱消息

**理由**：
- esp_mqtt 是 ESP-IDF 内置组件，支持 TLS 和 QoS
- TLS 保证传输安全（EMQX Cloud 强制要求）
- QoS 0（状态上报）：允许丢失，减少网络开销
- QoS 1（告警消息）：保证送达，确保关键信息不丢失
- 遗嘱消息：在设备异常掉线时自动发送离线状态

**替代方案考虑**：
- ❌ **使用第三方 MQTT 库（如 PubSubClient）**：ESP-IDF 内置组件已足够
- ❌ **使用 WebSocket over MQTT**：不必要的复杂性
- ❌ **使用 CoAP**：不是项目需求，且生态不如 MQTT

**实现细节**：
- Broker URL：`mqtts://xxx.emqxsl.cn:8883`（从 menuconfig 读取）
- 主题设计：
  - 状态上报：`home/ventilation/status`（QoS 0，每 30 秒发布一次）
  - 告警消息：`home/ventilation/alert`（QoS 1，触发时立即发布）
- 遗嘱消息：
  - 主题：`home/ventilation/status`
  - 内容：`{"online": false}`
  - QoS：1
- **自动重连策略**：
  - 使用 FreeRTOS 软件定时器实现延迟重连（30 秒超时，单次触发）
  - 在 MQTT 事件回调中检测到断开事件时启动定时器
  - 在定时器回调中执行实际重连操作（调用 `esp_mqtt_client_reconnect()`）
  - 避免在事件回调中阻塞（阻塞会导致其他 MQTT 事件无法处理）
- TLS 配置：使用 `esp_crt_bundle_attach` 自动加载 ESP-IDF 内置根证书
- **接口变更**：`mqtt_publish_status()` 增加 `SystemMode mode` 参数，用于上报当前运行模式

### 4. 错误处理和降级策略

**决策**：分层错误处理 + 平滑降级 + 详细日志

**理由**：
- 网络服务不稳定，必须提供健壮的错误处理
- 系统应在网络故障时继续运行（降级到本地模式）
- 详细日志便于调试和故障排查

**实现细节**：
- **WiFi 错误处理**：
  - 连接失败：记录错误日志，等待 10 秒后重试
  - 无限重试：避免短暂网络抖动导致系统失效
  - 用户可通过 `wifi_manager_is_connected()` 查询状态
- **天气 API 错误处理**：
  - HTTP 请求失败：返回 ESP_FAIL，调用方使用缓存数据
  - JSON 解析失败：返回 ESP_FAIL，调用方使用缓存数据
  - 超时：10 秒后返回 ESP_FAIL
- **MQTT 错误处理**：
  - 连接失败：记录错误日志，通过定时器在 30 秒后自动重连
  - 发布失败：返回 ESP_FAIL，调用方可选择重试或忽略
  - 不阻塞主逻辑：发布操作立即返回，不等待 ACK
  - 断线重连：使用 FreeRTOS 定时器异步处理，避免阻塞事件回调
- **系统降级策略**（在 main.c 中实现）：
  - 正常模式：WiFi 连接 + 天气数据有效
  - 降级模式：WiFi 断开但天气数据缓存有效（< 30 分钟）
  - 本地模式：天气数据缓存过期（> 30 分钟），仅基于 CO₂ 决策
  - 安全停机：传感器故障

### 5. 配置管理

**决策**：使用 menuconfig（Kconfig）+ sdkconfig.defaults

**理由**：
- menuconfig 是 ESP-IDF 标准配置方式，用户熟悉
- sdkconfig.defaults 提供默认配置（可选），便于开发和测试
- 配置项在编译时写入固件，运行时从 NVS 读取（WiFi 凭据）

**配置项设计**：
- WiFi：
  - `CONFIG_WIFI_SSID`（可选，开发用）
  - `CONFIG_WIFI_PASSWORD`（可选，开发用）
- 天气 API：
  - `CONFIG_WEATHER_API_KEY`（必需）
  - `CONFIG_WEATHER_CITY_CODE`（必需，如 "101010100" 代表北京）
- MQTT：
  - `CONFIG_MQTT_BROKER_URL`（必需，如 "mqtts://xxx.emqxsl.cn:8883"）
  - `CONFIG_MQTT_USERNAME`（必需）
  - `CONFIG_MQTT_PASSWORD`（必需）

### 6. 内存优化

**决策**：静态分配 + 有限缓冲区 + 及时释放资源

**理由**：
- ESP32-S3 SRAM 有限（~320KB），需要谨慎使用动态内存
- 静态分配避免碎片化
- 有限缓冲区避免 OOM

**实现细节**：
- HTTP 响应缓冲区：最大 2048 字节（和风天气 API 响应通常 < 1KB）
- MQTT 发布缓冲区：最大 512 字节（JSON 状态消息 < 256 字节）
- cJSON 对象：使用后立即调用 `cJSON_Delete()` 释放
- HTTP 客户端：使用后立即调用 `esp_http_client_cleanup()` 释放
- 避免在循环中频繁分配/释放内存

## Risks / Trade-offs

### 风险 1：网络不稳定导致频繁重连
- **影响**：消耗 CPU 和电源，影响传感器读取和决策
- **缓解**：
  - WiFi 重连间隔 10 秒（避免频繁尝试）
  - MQTT 重连间隔 30 秒
  - 重连逻辑在独立任务中执行，不阻塞主逻辑

### 风险 2：天气 API 限流或服务不可用
- **影响**：无法获取最新天气数据，影响决策质量
- **缓解**：
  - 实现 30 分钟缓存（减少 API 调用频率）
  - 降级到本地模式（仅基于 CO₂ 决策）
  - 获取间隔 10 分钟（远低于和风天气免费版限流：1000 次/天 ≈ 每分钟 0.7 次）

### 风险 3：TLS 证书验证失败
- **影响**：无法连接 HTTPS/MQTT TLS 服务
- **缓解**：
  - 使用 ESP-IDF 内置根证书（`esp_crt_bundle_attach`）
  - 测试时验证证书是否有效
  - 提供详细错误日志（证书链、验证失败原因）

### 风险 4：内存不足导致崩溃
- **影响**：系统崩溃或重启
- **缓解**：
  - 限制 HTTP/MQTT 缓冲区大小
  - 及时释放 cJSON 和 HTTP 客户端资源
  - 使用静态缓存代替动态分配
  - 在测试阶段监控内存使用（`esp_get_free_heap_size()`）

### 权衡 1：SmartConfig vs BLE 配网
- **选择**：SmartConfig
- **优势**：实现简单，无需额外代码
- **劣势**：需要用户下载 ESP Touch APP，兼容性可能有问题
- **结论**：对于原型和小规模部署，SmartConfig 足够；如需生产级配网，后续可扩展 BLE

### 权衡 2：QoS 0 vs QoS 1
- **选择**：状态上报使用 QoS 0，告警使用 QoS 1
- **优势**：平衡可靠性和性能（状态允许丢失，告警不允许）
- **劣势**：QoS 1 需要额外的 ACK 开销
- **结论**：符合实际需求，告警消息频率低，QoS 1 开销可接受

## Migration Plan

### 前置条件
- 确保 ESP-IDF 5.5.1 已正确安装
- 确保硬件支持 WiFi（ESP32-S3）

### 迁移步骤
1. **更新配置**：
   - 运行 `idf.py menuconfig`
   - 配置 WiFi、天气 API、MQTT 参数
   - 保存配置
2. **编译和烧录**：
   - 运行 `idf.py fullclean build`
   - 运行 `idf.py -p /dev/ttyACM0 flash monitor`
3. **首次配网**：
   - 观察日志，如提示"未找到 WiFi 配置"
   - 使用 ESP Touch APP 进行 SmartConfig 配网
   - 配网成功后，SSID/密码会保存到 NVS
4. **验证功能**：
   - 检查日志，确认 WiFi 连接成功
   - 检查日志，确认天气数据获取成功
   - 检查日志，确认 MQTT 连接成功
   - 使用 MQTT 客户端工具订阅 `home/ventilation/#` 验证消息

### 回滚方案
- 如果网络服务导致系统崩溃，可临时回退到桩函数版本
- 系统已实现降级和本地模式，网络故障不影响核心功能（CO₂ 监测和风扇控制）

## Open Questions

- [ ] 和风天气 API Key 如何分发？（开发/测试/生产环境）
  - **建议**：在文档中说明如何申请免费 API Key
- [ ] EMQX Cloud 凭据如何管理？（多设备部署时需要唯一 Client ID）
  - **建议**：使用 ESP32 MAC 地址作为 Client ID（`esp_read_mac()`）
- [ ] 是否需要支持多语言（天气 API 响应）？
  - **建议**：初期仅支持中文（lang=zh），后续可扩展
- [ ] 是否需要支持 IPv6？
  - **建议**：初期仅支持 IPv4，ESP-IDF 默认支持 IPv6 但需额外配置
- [ ] SmartConfig 超时时间设置为多少？
  - **建议**：60 秒（ESP Touch APP 通常在 30 秒内完成配网）
- [x] MQTT 订阅和远程控制功能是否在本次实现？
  - **决策**：**不实现**。本次仅实现状态发布和告警发布，订阅功能（`home/ventilation/command`）留待后续扩展。事件处理器中保留 MQTT_EVENT_SUBSCRIBED 处理作为预留接口。

## Implementation Notes

### 开发顺序
1. WiFi 管理器（优先级最高，其他模块依赖 WiFi）
2. 天气 API 客户端（决策算法依赖）
3. MQTT 客户端（状态上报，优先级较低）

### 测试策略
- **单元测试**：
  - JSON 解析逻辑（使用模拟 API 响应）
  - 缓存有效期检查（使用模拟时间戳）
  - FanState 转字符串逻辑
- **集成测试**：
  - WiFi 连接和重连（需要实际 AP）
  - 天气 API 获取（需要有效 API Key）
  - MQTT 发布（需要 EMQX Cloud 账号）
- **压力测试**：
  - 长时间运行（24 小时）监控内存泄漏
  - 频繁断网/恢复测试（WiFi 路由器开关）

### 日志规范
- 使用 ESP-IDF 标准日志宏（ESP_LOGI, ESP_LOGW, ESP_LOGE）
- 关键操作记录 INFO 级别日志（连接、断开、获取数据）
- 错误记录 ERROR 级别日志（包含错误码和原因）
- 调试信息记录 DEBUG 级别日志（HTTP 请求/响应、MQTT 消息）
- 日志中包含模块标签（"WIFI_MGR", "WEATHER_API", "MQTT_CLIENT"）

## References

- ESP-IDF WiFi 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/wifi.html
- ESP-IDF HTTP Client 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/protocols/esp_http_client.html
- ESP-IDF MQTT 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/protocols/mqtt.html
- 和风天气 API 文档：https://dev.qweather.com/docs/api/air/air-now/
- EMQX Cloud 文档：https://docs.emqx.com/zh/cloud/latest/
- cJSON 文档：https://github.com/DaveGamble/cJSON
- ESP Touch APP：https://www.espressif.com/zh-hans/products/software/esp-touch/overview
