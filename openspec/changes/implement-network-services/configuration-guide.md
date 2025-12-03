# 网络服务配置指南

本文档详细说明如何配置智能室内空气质量改善系统的网络服务功能，包括 WiFi、天气 API 和 MQTT 客户端。

---

## 📋 目录

1. [WiFi 配置](#1-wifi-配置)
2. [和风天气 API 配置](#2-和风天气-api-配置)
3. [EMQX Cloud MQTT 配置](#3-emqx-cloud-mqtt-配置)
4. [使用 menuconfig 配置](#4-使用-menuconfig-配置)
5. [常见问题和故障排除](#5-常见问题和故障排除)

---

## 1. WiFi 配置

系统支持两种 WiFi 配置方式：

### 方式 A：SmartConfig 配网（推荐首次使用）

SmartConfig 是 ESP32 的一键配网功能，无需修改代码即可配置 WiFi。

**步骤：**

1. **设备启动**：首次启动时，设备会检测到 NVS 中无 WiFi 配置，日志显示：
   ```
   W (576) WIFI_MGR: 未找到 WiFi 配置，请调用 wifi_manager_start_provisioning() 进行配网
   ```

2. **调用配网函数**：
   - 在代码中调用 `wifi_manager_start_provisioning()` 函数
   - 或通过串口命令触发配网（需自行实现命令解析）

3. **使用 ESP-Touch APP**：
   - 下载 ESP-Touch APP（iOS/Android）
   - 手机连接到目标 WiFi
   - 打开 APP，输入 WiFi 密码，点击"确认"
   - 等待 60 秒，设备会自动接收 WiFi 凭据并连接

4. **自动保存**：
   - 配网成功后，SSID 和密码会自动保存到 NVS
   - 下次启动会自动连接

**示例日志：**
```
I (1234) WIFI_MGR: 开始 SmartConfig 配网（60 秒超时）
I (5678) WIFI_MGR: SmartConfig 完成：SSID=MyWiFi
I (5680) WIFI_MGR: 正在连接到 WiFi...
I (7890) WIFI_MGR: WiFi 连接成功，IP: 192.168.1.100
```

---

### 方式 B：通过 menuconfig 手动配置（开发调试）

适用于开发阶段，直接将 WiFi 凭据编译到固件中。

**步骤：**

1. **运行 menuconfig**：
   ```bash
   idf.py menuconfig
   ```

2. **导航到配置菜单**：
   ```
   菜单路径：Component config → 网络服务配置 → WiFi 配置
   ```

3. **填写 WiFi 信息**（可选）：
   - `WiFi SSID（可选，用于开发测试）`：输入你的 WiFi 名称
   - `WiFi Password（可选）`：输入你的 WiFi 密码

4. **保存并退出**：
   - 按 `S` 保存，按 `Q` 退出

5. **重新编译和烧录**：
   ```bash
   idf.py build flash
   ```

**注意：** 此方式会将 WiFi 密码明文保存在二进制文件中，不推荐用于生产环境。

---

## 2. 和风天气 API 配置

和风天气 API 用于获取室外 PM2.5 浓度，辅助决策系统判断是否适合开窗通风。

### 2.1 申请 API Key

1. **注册账号**：
   - 访问 [和风天气开发者平台](https://dev.qweather.com/)
   - 注册免费账号

2. **创建项目**：
   - 登录后进入"控制台" → "项目管理"
   - 点击"创建项目"
   - 选择"Web API" 产品
   - 选择"免费订阅"（每天 1000 次调用）

3. **获取 API Key**：
   - 创建成功后，复制 API Key（格式类似：`1a2b3c4d5e6f7g8h9i0j`）

### 2.2 获取城市代码

1. **查询城市代码**：
   - 访问 [和风天气城市查询](https://github.com/qwd/LocationList/blob/master/China-City-List-latest.csv)
   - 搜索你的城市，复制对应的"Location ID"
   - 例如：北京 = `101010100`，上海 = `101020100`

### 2.3 配置到项目

使用 `idf.py menuconfig` 配置：

```
菜单路径：Component config → 网络服务配置 → 和风天气 API 配置
```

- `和风天气 API Key`：粘贴你的 API Key
- `城市代码`：输入你的城市 Location ID

保存后重新编译烧录：
```bash
idf.py build flash
```

**验证配置**：
启动后查看日志，应该显示：
```
I (646) WEATHER_API: 天气 API 客户端初始化完成
```

---

## 3. EMQX Cloud MQTT 配置

MQTT 用于将设备状态和告警上报到云端，支持远程监控。

### 3.1 创建 EMQX Cloud 账号

1. **注册账号**：
   - 访问 [EMQX Cloud](https://www.emqx.com/zh/cloud)
   - 注册免费账号（提供 14 天免费试用）

2. **创建部署**：
   - 登录后点击"新建部署"
   - 选择"Serverless" 计划（免费额度：100 万消息/月）
   - 选择区域（建议选择最近的区域，如"华东"）
   - 点击"创建"

3. **获取连接信息**：
   - 等待部署完成（约 5 分钟）
   - 进入部署详情页，记录以下信息：
     - **Broker URL**：格式类似 `mqtts://xxx.emqxsl.cn:8883`
     - **端口**：8883（mqtts TLS 加密端口）

### 3.2 创建认证凭据

1. **添加认证信息**：
   - 在部署详情页，点击"认证" → "添加"
   - 输入用户名（例如：`esp32_user`）
   - 输入密码（例如：`your_secure_password`）
   - 点击"确认"

2. **记录凭据**：
   - 用户名：`esp32_user`
   - 密码：`your_secure_password`

### 3.3 配置到项目

使用 `idf.py menuconfig` 配置：

```
菜单路径：Component config → 网络服务配置 → MQTT 配置
```

- `MQTT Broker URL`：输入 `mqtts://xxx.emqxsl.cn:8883`
- `MQTT 用户名`：输入你的用户名
- `MQTT 密码`：输入你的密码

保存后重新编译烧录：
```bash
idf.py build flash
```

**验证配置**：
启动后查看日志，应该显示：
```
I (656) MQTT_CLIENT: MQTT Client ID: esp32_3cdc75634378
I (696) MQTT_CLIENT: MQTT 客户端启动成功
```

WiFi 连接成功后，应该显示：
```
I (XXXX) MQTT_CLIENT: MQTT 连接成功
```

### 3.4 订阅主题（用于接收消息）

使用 MQTT 客户端工具（如 MQTTX）订阅以下主题：

- **状态主题**：`home/ventilation/status`
  - QoS：0
  - 发布频率：30 秒
  - 消息格式：
    ```json
    {
      "co2": 850,
      "temp": 24.5,
      "humi": 58,
      "fan_state": "LOW",
      "mode": "NORMAL",
      "timestamp": 1733241234
    }
    ```

- **告警主题**：`home/ventilation/alert`
  - QoS：1
  - 触发条件：CO₂ > 1500 ppm 或传感器故障
  - 消息格式：
    ```json
    {
      "alert": "CO₂浓度过高: 1600 ppm",
      "level": "WARNING",
      "timestamp": 1733241234
    }
    ```

---

## 4. 使用 menuconfig 配置

### 4.1 完整配置流程

1. **启动配置界面**：
   ```bash
   idf.py menuconfig
   ```

2. **导航到网络服务配置**：
   ```
   Component config → 网络服务配置
   ```

3. **依次配置三个子菜单**：
   - WiFi 配置（可选）
   - 和风天气 API 配置
   - MQTT 配置

4. **保存并退出**：
   - 按 `S` 保存
   - 按 `Q` 退出

5. **编译和烧录**：
   ```bash
   idf.py build flash monitor
   ```

### 4.2 配置文件位置

配置会保存在 `sdkconfig` 文件中，格式如下：
```ini
CONFIG_WIFI_SSID="MyWiFi"
CONFIG_WIFI_PASSWORD="MyPassword"
CONFIG_WEATHER_API_KEY="1a2b3c4d5e6f7g8h9i0j"
CONFIG_WEATHER_CITY_CODE="101010100"
CONFIG_MQTT_BROKER_URL="mqtts://xxx.emqxsl.cn:8883"
CONFIG_MQTT_USERNAME="esp32_user"
CONFIG_MQTT_PASSWORD="your_secure_password"
```

**注意：** `sdkconfig` 文件包含敏感信息，不要上传到公共仓库。

---

## 5. 常见问题和故障排除

### 5.1 WiFi 连接失败

**问题日志：**
```
E (5678) WIFI_MGR: 初始化阶段连接失败，已达最大重试次数
```

**可能原因和解决方案：**

1. **SSID 或密码错误**：
   - 检查 menuconfig 中的配置是否正确
   - 注意 WiFi 密码区分大小写
   - 使用 SmartConfig 重新配网

2. **WiFi 信号弱**：
   - 将设备移近路由器
   - 检查 WiFi 天线连接是否牢固

3. **路由器频段不匹配**：
   - ESP32-S3 仅支持 2.4GHz WiFi
   - 确保路由器开启了 2.4GHz 频段

4. **MAC 地址过滤**：
   - 在路由器中添加设备 MAC 地址到白名单
   - MAC 地址可从日志中查看：`wifi:mode : sta (3c:dc:75:63:43:78)`

---

### 5.2 MQTT 连接失败

**问题日志：**
```
E (666) esp-tls: couldn't get hostname for :xxx.emqxsl.cn: getaddrinfo() returns 202
E (666) esp-tls: Failed to open new connection
E (676) transport_base: Failed to open a new connection
E (676) mqtt_client: Error transport connect
```

**可能原因和解决方案：**

1. **WiFi 未连接**：
   - 确保 WiFi 已成功连接（查看日志是否有 `WiFi 连接成功`）
   - MQTT 客户端会在 WiFi 连接成功后自动重连

2. **Broker URL 配置错误**：
   - 检查 menuconfig 中的 `MQTT Broker URL` 是否正确
   - 确保使用 `mqtts://` 前缀（而非 `mqtt://`）
   - 端口号必须是 8883（TLS 加密端口）

3. **认证凭据错误**：
   - 检查用户名和密码是否与 EMQX Cloud 后台一致
   - 用户名和密码区分大小写

4. **TLS 证书问题**：
   - 确保 `esp_crt_bundle_attach` 已启用（已在代码中配置）
   - 如果使用自签名证书，需要手动添加证书到项目

5. **网络防火墙阻止**：
   - 检查公司/校园网络是否阻止了 8883 端口
   - 尝试使用手机热点测试

---

### 5.3 天气 API 获取失败

**问题日志：**
```
E (XXXX) WEATHER_API: HTTP 请求失败: ESP_ERR_HTTP_CONNECT
```

**可能原因和解决方案：**

1. **WiFi 未连接**：
   - 天气 API 需要网络连接
   - 检查 WiFi 连接状态

2. **API Key 无效**：
   - 检查 menuconfig 中的 API Key 是否正确
   - 确认 API Key 未过期（登录和风天气控制台查看）

3. **城市代码错误**：
   - 检查城市代码是否正确（参考 [城市列表](https://github.com/qwd/LocationList/blob/master/China-City-List-latest.csv)）

4. **API 调用次数超限**：
   - 免费版 API 每天 1000 次调用
   - 检查控制台的调用统计
   - 考虑升级订阅计划

5. **DNS 解析失败**：
   - 检查路由器 DNS 设置
   - 尝试使用公共 DNS（8.8.8.8 或 114.114.114.114）

---

### 5.4 SmartConfig 配网失败

**问题日志：**
```
W (60000) WIFI_MGR: SmartConfig 超时（60 秒），配网失败
```

**可能原因和解决方案：**

1. **手机未连接到目标 WiFi**：
   - 确保手机连接到要配置的 WiFi
   - 不支持手机使用 5GHz 频段

2. **ESP-Touch APP 版本过低**：
   - 更新到最新版 ESP-Touch APP
   - iOS 用户可尝试 EspTouch v2

3. **路由器隔离模式开启**：
   - 部分路由器开启了"客户端隔离"功能
   - 在路由器设置中关闭此功能

4. **超时时间不足**：
   - 默认 60 秒超时，可在代码中调整
   - 修改 `wifi_manager.c` 中的 `SMARTCONFIG_TIMEOUT_MS` 常量

---

### 5.5 系统进入降级模式

**问题日志：**
```
I (XXXX) MAIN: 系统运行模式：DEGRADED
```

**原因：**
- WiFi 连接成功，但天气 API 数据缓存过期（超过 30 分钟未更新）
- 系统会继续运行，但不使用室外 PM2.5 数据辅助决策

**解决方案：**
1. 检查天气 API 配置（参考 5.3 节）
2. 查看日志中的天气 API 错误信息
3. 确认网络连接稳定

---

### 5.6 系统进入本地模式

**问题日志：**
```
I (XXXX) MAIN: 系统运行模式：LOCAL
```

**原因：**
- WiFi 未连接或连接失败
- 系统会继续运行，仅使用室内传感器数据决策

**特点：**
- 不使用室外 PM2.5 数据
- 不发送 MQTT 状态和告警
- 决策算法回退到本地模式（只考虑室内 CO₂ 浓度）

**解决方案：**
1. 检查 WiFi 配置（参考 5.1 节）
2. 如果需要网络功能，重新配网

---

### 5.7 传感器读取失败

**问题日志：**
```
W (836) SENSOR_MGR: CO₂ 读取失败或超出范围，失败计数：1
E (836) SHT35: 发送测量命令失败 (-1)
W (836) SENSOR_MGR: SHT35 读取失败
```

**原因：**
- 传感器硬件未连接或接线错误
- I2C 总线故障

**解决方案：**
1. **检查硬件连接**：
   - CO₂ 传感器（UART）：GPIO16（TX）、GPIO17（RX）
   - SHT35 温湿度传感器（I2C）：GPIO21（SDA）、GPIO20（SCL）
   - 确认电源供电正常（3.3V 或 5V）

2. **I2C 地址检查**：
   - SHT35 默认地址：0x44
   - 使用 I2C 扫描工具验证地址

3. **传感器预热**：
   - CO₂ 传感器需要 60 秒预热时间
   - 系统会自动等待预热完成

**系统行为：**
- 传感器连续 3 次读取失败会触发降级
- 如果传感器恢复，系统会自动重新初始化

---

## 📚 相关资源

- [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)
- [和风天气 API 文档](https://dev.qweather.com/docs/api/)
- [EMQX Cloud 文档](https://docs.emqx.com/zh/cloud/latest/)
- [ESP-Touch APP 下载](https://www.espressif.com/zh-hans/products/software/esp-touch/resources)

---

## ✅ 配置完成检查清单

- [ ] WiFi 已配置并成功连接（日志显示 `WiFi 连接成功`）
- [ ] 天气 API 已配置（日志显示 `天气 API 客户端初始化完成`）
- [ ] MQTT 已配置并成功连接（日志显示 `MQTT 连接成功`）
- [ ] 系统进入 NORMAL 模式（日志显示 `系统运行模式：NORMAL`）
- [ ] MQTT 客户端工具能收到状态消息（订阅 `home/ventilation/status`）

如果所有检查项都通过，恭喜你完成了网络服务配置！🎉
