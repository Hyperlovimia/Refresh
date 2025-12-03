# 智能室内空气质量改善系统

基于 ESP32-S3 的智能新风控制系统，通过监测室内 CO₂ 浓度、温湿度以及室外 PM2.5 数据，自动控制新风机运行，改善室内空气质量。

---

## 📋 目录

- [功能特性](#功能特性)
- [硬件需求](#硬件需求)
- [软件依赖](#软件依赖)
- [快速开始](#快速开始)
- [网络服务配置](#网络服务配置)
- [系统架构](#系统架构)
- [项目结构](#项目结构)
- [开发指南](#开发指南)
- [故障排除](#故障排除)
- [参考资料](#参考资料)

---

## 功能特性

### 核心功能
- ✅ **智能决策**：基于室内 CO₂、温湿度和室外 PM2.5 数据自动控制新风机
- ✅ **多传感器支持**：CO₂ 传感器（UART）、SHT35 温湿度传感器（I2C）
- ✅ **OLED 显示**：实时显示传感器数据、系统状态和告警信息
- ✅ **三档风速控制**：OFF / LOW / HIGH，支持昼夜模式（夜间降低风速）

### 网络功能
- ✅ **WiFi 管理**：SmartConfig 一键配网 + NVS 凭据存储 + 自动重连
- ✅ **天气 API**：对接和风天气 API，获取室外 PM2.5 数据（30 分钟缓存）
- ✅ **MQTT 上报**：通过 EMQX Cloud 上报设备状态和告警（TLS 加密）
- ✅ **降级策略**：网络断开时自动切换到本地模式，保证系统稳定运行

### 系统特性
- ✅ **状态机管理**：INIT → PREHEATING (60s) → STABILIZING (240s) → RUNNING
- ✅ **传感器预热**：自动等待 CO₂ 传感器预热和稳定
- ✅ **错误恢复**：传感器故障自动检测和恢复
- ✅ **FreeRTOS 多任务**：传感器、决策、网络、显示任务并行运行

---

## 硬件需求

### 必需硬件
- **主控板**：ESP32-S3 开发板（推荐 ESP32-S3-WROOM-1）
- **CO₂ 传感器**：MH-Z19 或 JX-CO2-102（UART 接口）
- **温湿度传感器**：SHT35（I2C 接口）
- **显示屏**：SSD1306 OLED 128x64（I2C 接口）
- **新风机控制**：MOSFET 或继电器模块（PWM 控制）

### 可选硬件
- WiFi 天线（如果开发板未板载）
- USB-C 数据线（用于烧录和调试）

### 硬件连接

| 模块           | ESP32-S3 引脚 | 接口类型 | 说明                          |
|----------------|---------------|----------|-------------------------------|
| CO₂ 传感器     | GPIO16 (TX)   | UART     | 发送数据到传感器              |
|                | GPIO17 (RX)   | UART     | 接收传感器数据                |
| SHT35 温湿度   | GPIO21 (SDA)  | I2C      | I2C 数据线                    |
|                | GPIO20 (SCL)  | I2C      | I2C 时钟线                    |
| OLED 显示屏    | GPIO21 (SDA)  | I2C      | 与 SHT35 共享 I2C 总线        |
|                | GPIO20 (SCL)  | I2C      | I2C 地址：0x3C                |
| 风扇控制       | GPIO26        | PWM      | PWM 频率 25kHz，分辨率 8 位   |

**注意：** SHT35 和 OLED 共享 I2C 总线，需要确保地址不冲突（SHT35: 0x44, OLED: 0x3C）。

---

## 软件依赖

### 开发环境
- **ESP-IDF**: v5.5.1 或更高版本
- **Python**: 3.8 或更高版本
- **CMake**: 3.16 或更高版本

### ESP-IDF 组件
项目使用以下 ESP-IDF 组件：
- `driver`（UART、I2C、LEDC PWM）
- `esp_wifi`（WiFi STA 模式）
- `esp_netif`（网络接口）
- `nvs_flash`（WiFi 凭据存储）
- `esp_http_client`（HTTPS 客户端）
- `esp-tls`（TLS 加密）
- `mqtt`（MQTT 客户端）
- `json`（cJSON 库）
- `esp_timer`（定时器）

---

## 快速开始

### 1. 克隆项目
```bash
git clone <repository-url>
cd Refresh
```

### 2. 设置 ESP-IDF 环境
```bash
# 初始化 ESP-IDF 环境变量（根据你的 ESP-IDF 安装路径调整）
. $HOME/esp/v5.5.1/esp-idf/export.sh

# 或使用项目自定义命令（如果已配置）
get_idf
```

### 3. 配置目标芯片
```bash
idf.py set-target esp32s3
```

### 4. 配置项目（可选）
```bash
idf.py menuconfig
```
导航到 `Component config → 网络服务配置` 进行配置（详见 [网络服务配置](#网络服务配置)）。

### 5. 编译项目
```bash
idf.py build
```

### 6. 烧录固件
```bash
# 替换 /dev/ttyACM0 为你的实际串口设备
idf.py -p /dev/ttyACM0 flash
```

### 7. 查看日志
```bash
idf.py -p /dev/ttyACM0 monitor
```

按 `Ctrl+]` 退出监视器。

---

## 网络服务配置

系统支持三种网络服务：WiFi 连接、天气 API 和 MQTT 上报。完整配置指南请参考 [configuration-guide.md](openspec/changes/implement-network-services/configuration-guide.md)。

### 快速配置摘要

#### 1. WiFi 配置

**方式 A：SmartConfig 配网（推荐）**
1. 首次启动时，系统会提示未找到 WiFi 配置
2. 使用 ESP-Touch APP（iOS/Android）进行配网
3. 凭据自动保存到 NVS，下次启动自动连接

**方式 B：menuconfig 手动配置（开发调试）**
```bash
idf.py menuconfig
# 导航到：Component config → 网络服务配置 → WiFi 配置
```

#### 2. 和风天气 API 配置

1. 注册 [和风天气开发者账号](https://dev.qweather.com/)
2. 创建项目并获取 API Key
3. 查询城市代码（例如：北京 = 101010100）
4. 通过 menuconfig 配置：
   ```bash
   idf.py menuconfig
   # 导航到：Component config → 网络服务配置 → 和风天气 API 配置
   ```

**⚠️ 当前实现限制**：

由于和风天气的 `/air/now` 接口仅返回 PM2.5 数据，温度和风速使用默认值（20.0°C 和 0.0 m/s）。这不影响基于 PM2.5 的决策，但如果需要真实温度/风速，需要额外调用 `/weather/now` 接口。详见 [configuration-guide.md](openspec/changes/implement-network-services/configuration-guide.md#24-天气-api-限制说明)。

#### 3. MQTT 配置

1. 注册 [EMQX Cloud](https://www.emqx.com/zh/cloud) 账号
2. 创建 Serverless 部署（免费）
3. 获取 Broker URL（格式：`mqtts://xxx.emqxsl.cn:8883`）
4. 创建认证用户名和密码
5. 通过 menuconfig 配置：
   ```bash
   idf.py menuconfig
   # 导航到：Component config → 网络服务配置 → MQTT 配置
   ```

#### MQTT 主题说明

系统会发布以下主题：

- **状态主题**: `home/ventilation/status`
  - 发布频率：30 秒
  - QoS: 0
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

- **告警主题**: `home/ventilation/alert`
  - 触发条件：CO₂ > 1500 ppm 或传感器故障
  - QoS: 1
  - 消息格式：
    ```json
    {
      "alert": "CO₂浓度过高: 1600 ppm",
      "level": "WARNING",
      "timestamp": 1733241234
    }
    ```

---

## 系统架构

### 系统运行模式

系统支持 4 种运行模式，根据网络和传感器状态自动切换：

| 模式        | 描述                                                                 | 触发条件                                      |
|-------------|----------------------------------------------------------------------|-----------------------------------------------|
| NORMAL      | 正常模式：使用全部数据（室内+室外）进行决策                          | WiFi 连接 + 天气数据有效                      |
| DEGRADED    | 降级模式：使用室内数据 + 过期天气缓存                                 | WiFi 连接 + 天气数据过期（> 30 分钟）         |
| LOCAL       | 本地模式：仅使用室内传感器数据                                        | WiFi 断开                                     |
| SAFE_STOP   | 安全停机模式：传感器故障，关闭新风机                                  | 传感器健康检查失败                            |

### 状态机流程

```
INIT (首次启动或错误恢复)
  ↓
PREHEATING (60 秒 CO₂ 传感器预热)
  ↓
STABILIZING (240 秒传感器稳定)
  ↓
RUNNING (正常运行)
  ↓ (传感器故障)
ERROR (安全停机，每 10 秒尝试恢复)
  ↓ (传感器恢复)
INIT (重新初始化)
```

### FreeRTOS 任务架构

系统采用 4 个并行任务：

| 任务         | 优先级 | 频率   | 职责                                                                 |
|--------------|--------|--------|----------------------------------------------------------------------|
| sensor_task  | 高     | 1Hz    | 读取传感器数据，写入共享缓冲区，检查 CO₂ 告警                       |
| decision_task| 中     | 1Hz    | 读取传感器+天气数据，执行决策算法，控制风扇状态                      |
| network_task | 低     | 变频   | WiFi 管理，天气 API 拉取（10 分钟），MQTT 状态发布（30 秒）         |
| display_task | 低     | 0.5Hz  | 更新 OLED 显示，处理告警队列                                         |

### 同步机制

- **data_mutex**: 保护共享传感器数据、天气数据和风扇状态
- **i2c_mutex**: 保护 I2C 总线访问（SHT35 和 OLED 共享）
- **system_events**: 事件组，用于状态机同步（传感器就绪、WiFi 连接等）
- **alert_queue**: 告警消息队列，用于传感器任务向显示任务传递告警

---

## 项目结构

```
Refresh/
├── main/                      # 主应用组件
│   ├── main.c                 # 应用入口（app_main 函数）
│   ├── main.h                 # 全局类型和常量定义
│   ├── CMakeLists.txt         # 组件配置
│   ├── Kconfig.projbuild      # menuconfig 配置菜单
│   ├── actuators/             # 执行器控制模块
│   │   ├── fan_control.c      # 风扇 PWM 控制
│   │   └── fan_control.h
│   ├── algorithm/             # 决策算法模块
│   │   ├── decision_engine.c  # 决策引擎（模式检测 + 风扇控制）
│   │   ├── decision_engine.h
│   │   ├── local_mode.c       # 本地决策算法
│   │   └── local_mode.h
│   ├── network/               # 网络通信模块
│   │   ├── wifi_manager.c     # WiFi 管理（SmartConfig + 重连）
│   │   ├── wifi_manager.h
│   │   ├── weather_api.c      # 天气 API 客户端（HTTPS + JSON）
│   │   ├── weather_api.h
│   │   ├── mqtt_wrapper.c     # MQTT 客户端（TLS + 重连）
│   │   └── mqtt_wrapper.h
│   ├── sensors/               # 传感器接口模块
│   │   ├── co2_sensor.c       # CO₂ 传感器（UART）
│   │   ├── co2_sensor.h
│   │   ├── sht35.c            # SHT35 温湿度传感器（I2C）
│   │   ├── sht35.h
│   │   ├── sensor_manager.c   # 传感器管理器（健康检查 + 缓存）
│   │   └── sensor_manager.h
│   └── ui/                    # 用户界面模块
│       ├── oled_display.c     # OLED 显示（主页面 + 告警）
│       └── oled_display.h
├── openspec/                  # OpenSpec 文档和规范
│   ├── AGENTS.md              # OpenSpec 工作流指南
│   ├── project.md             # 项目元数据
│   ├── changes/               # 变更提案目录
│   │   └── implement-network-services/
│   │       ├── proposal.md    # 网络服务实现提案
│   │       ├── design.md      # 设计文档
│   │       ├── tasks.md       # 任务清单
│   │       └── configuration-guide.md  # 配置指南
│   └── specs/                 # 功能规范目录
│       ├── actuator-control/
│       ├── decision-algorithm/
│       ├── network-services/
│       ├── sensor-integration/
│       ├── system-orchestration/
│       └── user-interface/
├── build/                     # 构建产物（git-ignored）
├── CMakeLists.txt             # 项目级 CMake 配置
├── sdkconfig                  # ESP-IDF 配置文件（git-ignored）
├── README.md                  # 本文件
└── CLAUDE.md                  # AI 开发指南

```

---

## 开发指南

### 编译选项

**完整清理并重新编译**：
```bash
idf.py fullclean build
```

**仅编译（增量）**：
```bash
idf.py build
```

**设置详细日志级别**（调试用）：
```bash
idf.py menuconfig
# 导航到：Component config → Log output → Default log verbosity
# 选择 "Debug" 或 "Verbose"
```

### 烧录和监视

**合并操作**（编译+烧录+监视）：
```bash
idf.py -p /dev/ttyACM0 build flash monitor
```

**仅烧录**：
```bash
idf.py -p /dev/ttyACM0 flash
```

**仅监视**：
```bash
idf.py -p /dev/ttyACM0 monitor
```

### 修改分区表

如果固件大小超出默认分区，可修改分区配置：
```bash
idf.py menuconfig
# 导航到：Partition Table → Partition Table
# 选择 "Single factory app (large), no OTA"
```

---

## 故障排除

### 常见问题

#### 1. WiFi 连接失败
**症状**：日志显示 `初始化阶段连接失败，已达最大重试次数`

**解决方案**：
- 检查 SSID 和密码是否正确
- 确认 WiFi 为 2.4GHz 频段（ESP32 不支持 5GHz）
- 使用 SmartConfig 重新配网
- 检查路由器 MAC 地址过滤

#### 2. MQTT 连接失败
**症状**：日志显示 `couldn't get hostname for :xxx.emqxsl.cn`

**解决方案**：
- 确保 WiFi 已成功连接
- 检查 Broker URL 格式（必须是 `mqtts://xxx.emqxsl.cn:8883`）
- 验证用户名和密码是否正确
- 检查网络是否阻止 8883 端口

#### 3. 传感器读取失败
**症状**：日志显示 `CO₂ 读取失败` 或 `SHT35 发送测量命令失败`

**解决方案**：
- 检查传感器接线（参考硬件连接表）
- 确认传感器供电正常（3.3V 或 5V）
- CO₂ 传感器需要 60 秒预热时间
- 使用万用表测量 I2C 总线电压

#### 4. 天气 API 获取失败
**症状**：日志显示 `HTTP 请求失败: ESP_ERR_HTTP_CONNECT`

**解决方案**：
- 确保 WiFi 已连接
- 检查 API Key 是否有效（登录和风天气控制台）
- 验证城市代码是否正确
- 检查 API 调用次数是否超限（免费版每天 1000 次）

### 调试技巧

**启用详细日志**：
```bash
idf.py menuconfig
# 导航到：Component config → Log output → Default log verbosity → Verbose
```

**查看任务运行状态**（在代码中添加）：
```c
vTaskList(pcWriteBuffer);  // 需要开启 configUSE_TRACE_FACILITY
```

**监控堆内存使用**：
```c
ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
```

---

## 参考资料

### 官方文档
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)
- [ESP32-S3 技术规格书](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_cn.pdf)
- [和风天气 API 文档](https://dev.qweather.com/docs/api/)
- [EMQX Cloud 文档](https://docs.emqx.com/zh/cloud/latest/)

### 传感器文档
- MH-Z19 CO₂ 传感器数据手册
- SHT35 温湿度传感器数据手册
- SSD1306 OLED 显示器数据手册

### 工具和库
- [ESP-Touch APP](https://www.espressif.com/zh-hans/products/software/esp-touch/resources)（WiFi 配网工具）
- [MQTTX](https://mqttx.app/)（MQTT 客户端调试工具）
- [Postman](https://www.postman.com/)（HTTP API 测试工具）

---

## 许可证

本项目采用 MIT 许可证，详见 LICENSE 文件（如有）。

---

## 贡献

欢迎提交 Issue 和 Pull Request！

---

## 更新日志

### v1.0.0（2025-12-03）
- ✅ 完成网络服务实现（WiFi + 天气 API + MQTT）
- ✅ 实现传感器管理和决策引擎
- ✅ 实现 OLED 显示和风扇控制
- ✅ 编写完整配置指南和文档

---

**感谢使用智能室内空气质量改善系统！** 🎉
