# 提案：实现网络服务模块

## Why

当前网络服务模块（WiFi、天气 API、MQTT）仅实现了桩函数，无法提供实际网络功能。这导致：

1. **WiFi 无法连接**：系统无法接入网络，无法获取外部数据
2. **天气数据缺失**：决策算法依赖的室外 PM2.5、温度、风速数据无法获取
3. **状态无法上报**：设备运行状态和告警信息无法通过 MQTT 发送到云端监控
4. **降级模式失效**：系统无法在网络故障时切换到本地自主决策模式

实现完整的网络服务模块是系统正常运行的关键前提，使系统能够：
- 自动连接 WiFi 并在断线后重连
- 定期获取和风天气 API 数据并缓存
- 通过 MQTT 实时上报状态和告警到 EMQX Cloud
- 在网络故障时平滑切换到降级/本地模式

## What Changes

本变更将为三个网络服务模块提供完整实现：

### 1. WiFi 管理器（wifi_manager.c）
- **实现 WiFi 驱动初始化**：配置 STA 模式、NVS 存储、事件处理
- **实现 SmartConfig 配网**：支持首次配网和重新配网
- **实现连接状态管理**：自动连接、断线重连、状态查询
- **实现连接监控**：通过事件处理器跟踪 WiFi 状态变化

### 2. 天气 API 客户端（weather_api.c）
- **实现 HTTPS 客户端**：使用 esp_http_client 访问和风天气 API
- **实现 JSON 解析**：从 API 响应中提取 PM2.5、温度、风速数据
- **实现数据缓存**：本地缓存天气数据（30 分钟有效期）
- **实现缓存管理**：检查缓存有效性，在网络不可用时使用缓存

### 3. MQTT 客户端（mqtt_client.c）
- **实现 TLS 连接**：使用 esp_mqtt 连接到 EMQX Cloud（mqtts://）
- **实现状态发布**：定期发布传感器和风扇状态（JSON 格式，包含系统运行模式）
- **实现告警发布**：发布高优先级告警消息（QoS 1）
- **实现连接管理**：自动重连、遗嘱消息
- **接口变更**：`mqtt_publish_status()` 增加 `SystemMode mode` 参数，用于上报当前系统模式（NORMAL/DEGRADED/LOCAL/SAFE_STOP）

### 配置支持
- 新增 `sdkconfig.defaults` 配置：WiFi SSID/密码、API Key、MQTT 连接信息
- 通过 menuconfig 提供运行时配置接口

### 不包含的变更
- ❌ 不修改决策算法或传感器驱动
- ❌ 不实现 MQTT 订阅和远程控制功能（后续扩展）
- ❌ 不修改系统状态机或模式切换逻辑（main.c 已实现）

## Impact

### 受影响的能力
- **network-services**：从桩函数升级为完整实现

### 受影响的代码
- `main/network/wifi_manager.c`：~150 行实现（当前 28 行）
- `main/network/weather_api.c`：~200 行实现（当前 63 行）
- `main/network/mqtt_client.h`：接口修改（`mqtt_publish_status` 增加 `SystemMode mode` 参数）
- `main/network/mqtt_client.c`：~180 行实现（当前 36 行）
- `main/main.c`：调用 `mqtt_publish_status` 时需传递 SystemMode 参数（约 1 处修改）
- `CMakeLists.txt`（根目录）：可能需要添加 esp_http_client、esp_mqtt 组件依赖
- `sdkconfig.defaults`：新增配置项（如不存在则创建）

### 外部依赖
- ESP-IDF 内置组件：
  - `esp_wifi`：WiFi 驱动
  - `esp_netif`：网络接口抽象层
  - `nvs_flash`：WiFi 配置存储
  - `esp_http_client`：HTTPS 客户端
  - `esp_mqtt`：MQTT 客户端
  - `cJSON`：JSON 解析
  - `esp_crt_bundle`：TLS 根证书（HTTPS 和 MQTT）

### 测试影响
- 需要实际硬件和网络环境进行集成测试
- 单元测试可覆盖数据解析、缓存逻辑等纯算法部分
- 需要配置有效的 WiFi 接入点、和风天气 API Key、EMQX Cloud 连接信息

### 风险和缓解
1. **网络不稳定导致测试困难**
   - 缓解：实现健壮的重连逻辑，添加详细日志，在稳定环境下测试
2. **API Key 和 MQTT 凭据安全**
   - 缓解：通过 menuconfig 配置，不提交到版本控制，文档中说明配置方法
3. **HTTPS 证书验证失败**
   - 缓解：启用 `esp_crt_bundle_attach`，使用 ESP-IDF 内置根证书
4. **内存占用增加**
   - 缓解：优化 HTTP/MQTT 缓冲区大小，避免过度分配

## Implementation Plan

### 阶段 1：WiFi 管理器实现（优先级：高）
- 实现 WiFi 初始化和驱动配置
- 实现事件处理器（连接、断开、获取 IP）
- 实现 NVS 读写和自动连接逻辑
- 实现 SmartConfig 配网（阻塞模式，60 秒超时）
- 验证：编译通过，日志输出正确，可连接到测试 AP

### 阶段 2：天气 API 客户端实现（优先级：高）
- 实现 HTTPS 客户端初始化和请求
- 实现 JSON 响应解析（cJSON）
- 实现数据缓存和有效期检查
- 实现错误处理和超时逻辑
- 验证：编译通过，成功获取天气数据，缓存正常工作

### 阶段 3：MQTT 客户端实现（优先级：中）
- 实现 MQTT 客户端初始化和 TLS 配置
- 实现状态发布（JSON 构建和发送）
- 实现告警发布（QoS 1）
- 实现事件处理器（连接、断开、发布确认）
- 验证：编译通过，成功连接 Broker，消息正常发布

### 阶段 4：配置文件和文档（优先级：低）
- 创建 `sdkconfig.defaults`（或更新现有文件）
- 编写配置说明文档（WiFi、API Key、MQTT）
- 更新 README.md（网络服务配置章节）

### 阶段 5：集成测试（优先级：高）
- 测试 WiFi 连接、断线重连
- 测试天气 API 获取、缓存、过期
- 测试 MQTT 发布、重连
- 测试网络降级模式（断开 WiFi 后使用缓存）
- 验证系统状态机正确切换（正常→降级→本地）

## Success Criteria

- [ ] WiFi 可成功连接到配置的接入点
- [ ] WiFi 初始化时连接失败可正确重试（5 秒间隔，最多 3 次，失败后返回 ESP_FAIL）
- [ ] WiFi 运行时断开后可自动重连（10 秒间隔，无限重试）
- [ ] SmartConfig 配网功能正常（可通过 ESP Touch APP 配网）
- [ ] 天气 API 可成功获取数据（PM2.5、温度、风速）
- [ ] 天气数据缓存有效期正确（30 分钟）
- [ ] 网络不可用时可使用缓存数据
- [ ] MQTT 可成功连接到 EMQX Cloud（TLS）
- [ ] MQTT 可正确发布状态消息（QoS 0）
- [ ] MQTT 可正确发布告警消息（QoS 1）
- [ ] MQTT 断开后可自动重连（30 秒间隔）
- [ ] 代码编译通过，无警告
- [ ] 所有日志输出清晰、易于调试
- [ ] 配置说明文档完整、准确

## Approval Requirements

- [ ] 实现方案经过审查（WiFi、HTTPS、MQTT 技术选型）
- [ ] 安全措施可接受（TLS、凭据管理）
- [ ] 错误处理和重连逻辑健壮
- [ ] 测试计划完整（单元测试 + 集成测试）
- [ ] 依赖关系明确（ESP-IDF 组件）

## References

- ESP-IDF WiFi 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/wifi.html
- ESP-IDF HTTP Client 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/protocols/esp_http_client.html
- ESP-IDF MQTT 文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/protocols/mqtt.html
- 和风天气 API 文档：https://dev.qweather.com/docs/api/
- EMQX Cloud 文档：https://docs.emqx.com/zh/cloud/latest/
- 现有规格说明：`openspec/specs/network-services/spec.md`
