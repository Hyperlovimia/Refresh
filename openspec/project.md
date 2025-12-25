# 智能室内空气质量改善系统 - 项目介绍

## 项目概述

本项目是一个基于 ESP32-S3 微控制器的智能新风控制系统，通过实时监测室内 CO₂ 浓度、温湿度等环境参数，结合云端远程控制能力，自动调节新风机运行状态，实现室内空气质量的智能化管理。

系统采用 FreeRTOS 实时操作系统，实现多任务并行处理，支持本地自动决策和云端远程控制两种运行模式，具备完善的故障检测与安全保护机制。

---

## 技术栈

### 硬件平台
- **主控芯片**: ESP32-S3-WROOM-1（双核 Xtensa LX7，240MHz）
- **CO₂ 传感器**: JX-CO2-102（UART 接口，NDIR 红外检测）
- **温湿度传感器**: SHT35（I2C 接口，高精度数字传感器）
- **显示屏**: SSD1306 OLED 128×64（I2C 接口）
- **执行器**: 30mm PWM 风扇模块 ×3（25kHz PWM 控制）

### 软件框架
- **开发框架**: ESP-IDF v5.5.1
- **实时系统**: FreeRTOS（多任务调度）
- **图形库**: u8g2（OLED 显示驱动）
- **通信协议**: MQTT over TLS（EMQX Cloud）
- **配网方式**: SmartConfig（ESP-Touch）

### 开发语言
- C 语言（ESP-IDF 原生开发）

---

## 系统架构

### 模块划分

```
┌─────────────────────────────────────────────────────────────┐
│                        main.c                               │
│                   (系统入口 + 状态机)                        │
├─────────────────────────────────────────────────────────────┤
│  sensors/          │  algorithm/       │  actuators/        │
│  ├─ sensor_manager │  ├─ decision_     │  └─ fan_control    │
│  ├─ co2_sensor     │  │   engine       │     (PWM 控制)     │
│  └─ sht35          │  └─ local_mode    │                    │
├─────────────────────────────────────────────────────────────┤
│  network/                    │  ui/                         │
│  ├─ wifi_manager             │  ├─ oled_display             │
│  └─ mqtt_wrapper             │  └─ u8g2_esp32_hal           │
└─────────────────────────────────────────────────────────────┘
```

### 任务架构

系统采用 4 个 FreeRTOS 任务并行运行：

| 任务名称 | 优先级 | 周期 | 职责 |
|---------|--------|------|------|
| sensor_task | 高(3) | 1Hz | 读取传感器数据，检测 CO₂ 告警 |
| decision_task | 中(3) | 1Hz | 模式检测，执行决策，控制风扇 |
| network_task | 低(2) | 30s | WiFi 状态管理，MQTT 状态上报 |
| display_task | 低(2) | 0.5Hz | OLED 显示更新，告警展示 |

### 状态机设计

```
INIT (系统初始化)
  ↓
PREHEATING (60秒 CO₂ 传感器预热)
  ↓
STABILIZING (240秒 传感器稳定)
  ↓
RUNNING (正常运行)
  ↓ (传感器故障)
ERROR (安全停机，每10秒尝试恢复)
  ↓ (传感器恢复)
INIT (重新初始化)
```

---

## 核心功能

### 1. 传感器数据采集

- **CO₂ 监测**: UART 通信，支持预热等待和数据缓存
- **温湿度监测**: I2C 高精度采集，与 OLED 共享总线
- **健康检查**: 连续失败 3 次判定为故障，自动进入安全模式
- **数据有效性**: 范围校验（CO₂: 300-5000ppm，温度: -10~50℃）

### 2. 智能决策引擎

**三种运行模式**：

| 模式 | 触发条件 | 决策逻辑 |
|------|---------|---------|
| MODE_REMOTE | WiFi 已连接 | 接收云端风扇控制命令 |
| MODE_LOCAL | WiFi 断开 | 基于 CO₂ 阈值本地决策 |
| MODE_SAFE_STOP | 传感器故障 | 强制关闭所有风扇 |

**本地决策阈值**：
- CO₂ < 1000ppm → 风扇关闭
- 1000ppm ≤ CO₂ < 1200ppm → 低速运行
- CO₂ ≥ 1200ppm → 高速运行
- CO₂ > 1500ppm → 触发告警

### 3. 风扇控制

- **三档调速**: OFF / LOW / HIGH
- **PWM 控制**: 25kHz 频率，8位分辨率
- **昼夜模式**: 22:00-8:00 自动降低风速（减少噪音）
- **多风扇支持**: 3 路独立 PWM 输出（GPIO26/27/33）

### 4. 网络通信

**WiFi 管理**：
- SmartConfig 一键配网
- NVS 凭据持久化存储
- 断线自动重连机制

**MQTT 双向通信**：
- Broker: EMQX Cloud（TLS 加密）
- 状态上报: `home/ventilation/status`（30秒周期）
- 告警推送: `home/ventilation/alert`（QoS=1）
- 远程控制: `home/ventilation/command`（支持部分更新）

### 5. 用户界面

**OLED 显示内容**：
- 第1行: CO₂ 数值 + 运行模式徽章
- 第2行: 温度 + 湿度
- 第3-6行: CO₂ 历史趋势图（10分钟采样）
- 第7-8行: 风扇状态 + WiFi 连接状态

**告警显示**：
- 全屏告警页面（2秒展示）
- 告警队列机制（最多缓存5条）

---

## 数据结构

### 传感器数据
```c
typedef struct {
    IndoorPollutants pollutants;  // CO₂/PM/VOC/HCHO
    float temperature;            // 温度（℃）
    float humidity;               // 湿度（%）
    time_t timestamp;             // 时间戳
    bool valid;                   // 有效标志
} SensorData;
```

### MQTT 状态消息
```json
{
  "co2": 850,
  "temp": 24.5,
  "humi": 58,
  "fan_0": "LOW",
  "fan_1": "HIGH",
  "fan_2": "OFF",
  "mode": "REMOTE",
  "timestamp": 1701936000
}
```

---

## 同步机制

| 共享资源 | 保护方式 | 访问者 |
|---------|---------|--------|
| shared_sensor_data | data_mutex | 所有任务 |
| shared_fan_states | data_mutex | decision/network/display |
| I2C 总线 | i2c_mutex | SHT35/OLED |
| 告警消息 | alert_queue | sensor→display |
| 系统事件 | system_events | 状态机同步 |

---

## 项目约定

### 代码风格
- 函数命名: `模块名_动作_对象`（如 `sensor_manager_read_all`）
- 常量命名: 全大写下划线分隔（如 `CO2_THRESHOLD_HIGH`）
- 日志标签: 模块名大写（如 `MAIN`, `MQTT_CLIENT`）

### 错误处理
- 所有初始化函数返回 `esp_err_t`
- 非关键模块初始化失败不阻塞系统启动
- 传感器故障自动进入安全停机模式

### Git 工作流
- 主分支: `main`
- 功能开发: `feature/xxx`
- 提交信息: 中文描述，简洁明了

---

## 硬件接线

| 模块 | ESP32-S3 引脚 | 接口 | 备注 |
|------|--------------|------|------|
| CO₂ 传感器 TX | GPIO17 | UART | ESP32 发送命令 |
| CO₂ 传感器 RX | GPIO16 | UART | ESP32 接收数据 |
| SHT35 SDA | GPIO21 | I2C | 地址 0x44 |
| SHT35 SCL | GPIO20 | I2C | 共享总线 |
| OLED SDA | GPIO21 | I2C | 地址 0x3C |
| OLED SCL | GPIO20 | I2C | 共享总线 |
| 风扇0 PWM | GPIO36 | LEDC | 25kHz |
| 风扇1 PWM | GPIO37 | LEDC | 25kHz |
| 风扇2 PWM | GPIO38 | LEDC | 25kHz |

---

## 外部依赖

### ESP-IDF 组件
- `driver`（UART/I2C/LEDC）
- `esp_wifi` + `esp_netif`
- `nvs_flash`
- `mqtt`
- `esp-tls`（TLS 加密）
- `cJSON`

### 第三方库
- **u8g2**: OLED 图形库（作为 git submodule 引入）

### 云服务
- **EMQX Cloud**: MQTT Broker（Serverless 免费版）

---

## 重要约束

### 技术约束
- ESP32-S3 不支持 5GHz WiFi
- CO₂ 传感器需要 60 秒预热 + 240 秒稳定
- I2C 总线最大速率 400kHz

### 安全约束
- MQTT 必须使用 TLS 加密（端口 8883）
- WiFi 凭据存储在 NVS 加密分区
- 传感器故障时强制关闭风扇

### 资源约束
- Flash 分区: 单 factory app（无 OTA）
- 任务栈: 小任务 4KB，网络任务 8KB
- 告警队列: 最大 5 条消息

---

## 版本历史

### v1.0.0 (2025-12-03)
- ✅ 完成传感器集成（CO₂ + SHT35）
- ✅ 实现决策引擎（本地 + 远程模式）
- ✅ 实现网络服务（WiFi + MQTT）
- ✅ 实现 OLED 显示（主页面 + 趋势图 + 告警）
- ✅ 实现多风扇 PWM 控制（3路独立）
- ✅ 完成系统状态机和错误恢复

---

## 文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| 项目 README | `/README.md` | 快速开始指南 |
| MQTT 协议 | `/Guides/MQTT.md` | 消息格式与数据流 |
| CO₂ 传感器手册 | `/Guides/JX_CO2_102手册.md` | 硬件通信协议 |
| 功能规格 | `/openspec/specs/` | 各模块设计文档 |

---

## 联系方式

如有问题或建议，欢迎提交 Issue 或 Pull Request。
