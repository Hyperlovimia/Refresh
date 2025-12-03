# 实施任务清单

## 1. WiFi 管理器实现

- [ ] 1.1 实现 `wifi_manager_init()` 函数
  - [ ] 1.1.1 初始化 NVS（nvs_flash_init）
  - [ ] 1.1.2 初始化 netif（esp_netif_init）
  - [ ] 1.1.3 创建事件循环（esp_event_loop_create_default）
  - [ ] 1.1.4 创建默认 WiFi STA netif
  - [ ] 1.1.5 初始化 WiFi 驱动（WIFI_INIT_CONFIG_DEFAULT）
  - [ ] 1.1.6 注册事件处理器（WIFI_EVENT, IP_EVENT）
  - [ ] 1.1.7 配置 WiFi 为 STA 模式
  - [ ] 1.1.8 从 NVS 读取保存的 SSID/密码
  - [ ] 1.1.9 如果有配置，调用 esp_wifi_start() 和 esp_wifi_connect()

- [ ] 1.2 实现事件处理器 `wifi_event_handler()`
  - [ ] 1.2.1 处理 WIFI_EVENT_STA_START（WiFi 启动）
  - [ ] 1.2.2 处理 WIFI_EVENT_STA_CONNECTED（连接成功）
  - [ ] 1.2.3 处理 WIFI_EVENT_STA_DISCONNECTED（连接断开，触发重连）
  - [ ] 1.2.4 处理 IP_EVENT_STA_GOT_IP（获取 IP 地址）
  - [ ] 1.2.5 使用 EventGroupBits 标记连接状态

- [ ] 1.3 实现 `wifi_manager_start_provisioning()` 函数
  - [ ] 1.3.1 初始化 SmartConfig（esp_smartconfig_start）
  - [ ] 1.3.2 等待 SmartConfig 完成（阻塞模式，60 秒超时）
  - [ ] 1.3.3 接收 SSID/密码并保存到 NVS
  - [ ] 1.3.4 自动连接到配置的 WiFi
  - [ ] 1.3.5 停止 SmartConfig

- [ ] 1.4 实现 `wifi_manager_is_connected()` 函数
  - [ ] 1.4.1 检查 EventGroupBits 中的连接标志
  - [ ] 1.4.2 返回连接状态（true/false）

- [ ] 1.5 添加必要的头文件和宏定义
  - [ ] 1.5.1 esp_wifi.h, esp_event.h, esp_netif.h, nvs_flash.h
  - [ ] 1.5.2 定义 WIFI_CONNECTED_BIT 和 WIFI_FAIL_BIT
  - [ ] 1.5.3 定义重试间隔和最大重试次数常量

- [ ] 1.6 验证 WiFi 管理器
  - [ ] 1.6.1 编译通过，无警告
  - [ ] 1.6.2 日志输出清晰（连接、断开、重连）
  - [ ] 1.6.3 可成功连接到测试 AP
  - [ ] 1.6.4 断线后可自动重连

## 2. 天气 API 客户端实现

- [ ] 2.1 实现 `weather_api_init()` 函数
  - [ ] 2.1.1 从 menuconfig 读取 API Key 和城市代码
  - [ ] 2.1.2 初始化 HTTPS 客户端配置（esp_http_client_config_t）
  - [ ] 2.1.3 启用 TLS（esp_crt_bundle_attach）
  - [ ] 2.1.4 清空缓存数据

- [ ] 2.2 实现 `weather_api_fetch()` 函数
  - [ ] 2.2.1 构建 API 请求 URL（https://api.qweather.com/v7/air/now?location=XXX&key=XXX）
  - [ ] 2.2.2 创建 HTTP 客户端（esp_http_client_init）
  - [ ] 2.2.3 执行 HTTP GET 请求
  - [ ] 2.2.4 读取响应数据到缓冲区
  - [ ] 2.2.5 检查 HTTP 状态码（200 OK）
  - [ ] 2.2.6 清理 HTTP 客户端（esp_http_client_cleanup）

- [ ] 2.3 实现 JSON 解析逻辑
  - [ ] 2.3.1 使用 cJSON 解析响应体
  - [ ] 2.3.2 提取 "now" -> "pm2p5" 字段（PM2.5 浓度）
  - [ ] 2.3.3 提取 "now" -> "temp" 字段（温度）
  - [ ] 2.3.4 提取 "now" -> "windSpeed" 字段（风速）
  - [ ] 2.3.5 验证字段有效性（非空、类型正确）
  - [ ] 2.3.6 填充 WeatherData 结构体
  - [ ] 2.3.7 设置时间戳和有效标志
  - [ ] 2.3.8 释放 cJSON 对象

- [ ] 2.4 实现数据缓存逻辑
  - [ ] 2.4.1 在 `weather_api_fetch()` 成功后保存到静态变量
  - [ ] 2.4.2 记录缓存时间戳（gettimeofday）
  - [ ] 2.4.3 实现 `weather_api_get_cached()` 函数（memcpy）
  - [ ] 2.4.4 实现 `weather_api_is_cache_stale()` 函数（检查时间差 > 30 分钟）

- [ ] 2.5 实现错误处理
  - [ ] 2.5.1 检查 HTTP 客户端初始化失败
  - [ ] 2.5.2 检查 HTTP 请求超时（10 秒）
  - [ ] 2.5.3 检查 HTTP 状态码非 200
  - [ ] 2.5.4 检查 JSON 解析失败
  - [ ] 2.5.5 记录错误日志（ESP_LOGE）

- [ ] 2.6 验证天气 API 客户端
  - [ ] 2.6.1 编译通过，无警告
  - [ ] 2.6.2 成功获取天气数据（使用有效 API Key）
  - [ ] 2.6.3 JSON 解析正确（PM2.5、温度、风速）
  - [ ] 2.6.4 缓存正常工作（重复调用使用缓存）
  - [ ] 2.6.5 缓存过期检查正确（30 分钟）

## 3. MQTT 客户端实现

- [ ] 3.0 更新 mqtt_client.h 接口定义
  - [ ] 3.0.1 修改 `mqtt_publish_status()` 函数签名，增加 `SystemMode mode` 参数
  - [ ] 3.0.2 更新函数注释，说明 mode 参数用于状态上报

- [ ] 3.1 实现 `mqtt_client_init()` 函数
  - [ ] 3.1.1 从 menuconfig 读取 MQTT Broker URL、用户名、密码
  - [ ] 3.1.2 配置 MQTT 客户端（esp_mqtt_client_config_t）
  - [ ] 3.1.3 设置 TLS 启用（transport = MQTT_TRANSPORT_OVER_SSL）
  - [ ] 3.1.4 启用根证书验证（esp_crt_bundle_attach）
  - [ ] 3.1.5 设置遗嘱消息（Last Will）：home/ventilation/status -> {"online": false}
  - [ ] 3.1.6 注册事件处理器（esp_mqtt_client_register_event）
  - [ ] 3.1.7 启动 MQTT 客户端（esp_mqtt_client_start）

- [ ] 3.2 实现事件处理器 `mqtt_event_handler()`
  - [ ] 3.2.1 处理 MQTT_EVENT_CONNECTED（连接成功）
  - [ ] 3.2.2 处理 MQTT_EVENT_DISCONNECTED（连接断开）
  - [ ] 3.2.3 处理 MQTT_EVENT_SUBSCRIBED（预留，用于未来远程控制功能）
  - [ ] 3.2.4 处理 MQTT_EVENT_PUBLISHED（发布成功）
  - [ ] 3.2.5 处理 MQTT_EVENT_ERROR（错误）
  - [ ] 3.2.6 使用标志位标记连接状态

- [ ] 3.3 实现 `mqtt_publish_status()` 函数
  - [ ] 3.3.1 检查 MQTT 客户端连接状态
  - [ ] 3.3.2 构建 JSON 消息：{"co2": XXX, "temp": XXX, "humi": XXX, "fan_state": "XXX", "mode": "XXX", "timestamp": XXX}
  - [ ] 3.3.3 将 FanState 枚举转换为字符串（"OFF", "LOW", "HIGH"）
  - [ ] 3.3.4 将 SystemMode 枚举转换为字符串（"NORMAL", "DEGRADED", "LOCAL", "SAFE_STOP"）
  - [ ] 3.3.5 使用 cJSON 构建 JSON 字符串
  - [ ] 3.3.6 发布到主题 "home/ventilation/status"，QoS 0
  - [ ] 3.3.7 释放 cJSON 对象

- [ ] 3.4 实现 `mqtt_publish_alert()` 函数
  - [ ] 3.4.1 检查 MQTT 客户端连接状态
  - [ ] 3.4.2 构建 JSON 消息：{"alert": "XXX", "level": "WARNING", "timestamp": XXX}
  - [ ] 3.4.3 使用 cJSON 构建 JSON 字符串
  - [ ] 3.4.4 发布到主题 "home/ventilation/alert"，QoS 1（保证送达）
  - [ ] 3.4.5 释放 cJSON 对象

- [ ] 3.5 实现自动重连逻辑
  - [ ] 3.5.1 创建 FreeRTOS 软件定时器用于延迟重连（30 秒超时，单次触发）
  - [ ] 3.5.2 在事件处理器中处理 MQTT_EVENT_DISCONNECTED 断开事件
  - [ ] 3.5.3 在断开事件中启动重连定时器（xTimerStart），避免在回调中阻塞
  - [ ] 3.5.4 在定时器回调中调用 esp_mqtt_client_reconnect()
  - [ ] 3.5.5 重连成功后停止定时器（如已在重连中）

- [ ] 3.6 验证 MQTT 客户端
  - [ ] 3.6.1 编译通过，无警告
  - [ ] 3.6.2 成功连接到 EMQX Cloud Broker（TLS）
  - [ ] 3.6.3 成功发布状态消息（QoS 0）
  - [ ] 3.6.4 成功发布告警消息（QoS 1）
  - [ ] 3.6.5 断线后可自动重连
  - [ ] 3.6.6 使用 MQTT 客户端工具验证消息格式正确

## 4. 配置支持

- [ ] 4.1 创建或更新 `sdkconfig.defaults`
  - [ ] 4.1.1 添加 WiFi SSID 配置项（可选，用于开发）
  - [ ] 4.1.2 添加 WiFi 密码配置项（可选）
  - [ ] 4.1.3 添加和风天气 API Key 配置项
  - [ ] 4.1.4 添加和风天气城市代码配置项
  - [ ] 4.1.5 添加 MQTT Broker URL 配置项
  - [ ] 4.1.6 添加 MQTT 用户名配置项
  - [ ] 4.1.7 添加 MQTT 密码配置项

- [ ] 4.2 更新 Kconfig.projbuild（如需要）
  - [ ] 4.2.1 定义 WiFi 配置菜单项
  - [ ] 4.2.2 定义天气 API 配置菜单项
  - [ ] 4.2.3 定义 MQTT 配置菜单项

- [ ] 4.3 检查 CMakeLists.txt 依赖
  - [ ] 4.3.1 确认 esp_http_client 组件已包含
  - [ ] 4.3.2 确认 esp_mqtt 组件已包含
  - [ ] 4.3.3 确认 json 组件已包含
  - [ ] 4.3.4 确认 esp-tls 组件已包含

## 5. 文档和测试

- [ ] 5.1 编写配置说明文档
  - [ ] 5.1.1 WiFi 配置方法（SmartConfig + 手动配置）
  - [ ] 5.1.2 和风天气 API Key 申请和配置
  - [ ] 5.1.3 EMQX Cloud 账号创建和连接信息获取
  - [ ] 5.1.4 menuconfig 配置步骤说明

- [ ] 5.2 更新 README.md
  - [ ] 5.2.1 添加网络服务配置章节
  - [ ] 5.2.2 添加首次运行步骤
  - [ ] 5.2.3 添加常见问题和故障排除

- [ ] 5.3 集成测试
  - [ ] 5.3.1 测试 WiFi 连接和重连
  - [ ] 5.3.2 测试天气 API 获取和缓存
  - [ ] 5.3.3 测试 MQTT 发布和重连
  - [ ] 5.3.4 测试网络降级模式（断开 WiFi）
  - [ ] 5.3.5 测试系统状态机切换（正常→降级→本地）
  - [ ] 5.3.6 记录测试结果和日志

- [ ] 5.4 代码审查
  - [ ] 5.4.1 检查错误处理完整性
  - [ ] 5.4.2 检查资源释放（内存泄漏）
  - [ ] 5.4.3 检查日志输出清晰度
  - [ ] 5.4.4 检查代码风格一致性
  - [ ] 5.4.5 检查注释完整性

## 6. 验证和交付

- [ ] 6.1 编译验证
  - [ ] 6.1.1 执行 `idf.py fullclean build`
  - [ ] 6.1.2 确认无编译错误和警告
  - [ ] 6.1.3 检查二进制文件大小合理

- [ ] 6.2 功能验证
  - [ ] 6.2.1 WiFi 功能完全正常
  - [ ] 6.2.2 天气 API 功能完全正常
  - [ ] 6.2.3 MQTT 功能完全正常
  - [ ] 6.2.4 网络降级和本地模式正常

- [ ] 6.3 文档验证
  - [ ] 6.3.1 配置说明准确无误
  - [ ] 6.3.2 README 更新完整

- [ ] 6.4 提交变更
  - [ ] 6.4.1 更新 tasks.md 中所有任务为已完成
  - [ ] 6.4.2 准备提交审批
