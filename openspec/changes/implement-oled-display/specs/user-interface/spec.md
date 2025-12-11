## MODIFIED Requirements

### Requirement: OLED 显示初始化

OLED 显示模块 MUST 提供初始化功能，配置 I2C 通信和显示参数。

#### Scenario: 初始化 OLED 显示

**Given** ESP32-S3 GPIO21/22 已配置为 I2C SDA/SCL（与 SHT35 共享）
**When** 调用 `oled_display_init()`
**Then**
- 初始化 u8g2 库（使用 `u8g2_Setup_ssd1306_i2c_128x64_noname_f` 全缓冲模式）
- 配置 I2C 地址为 `0x3C`
- 设置默认字体为 `u8g2_font_6x10_tf`（6x10 像素）
- 清空显示缓冲区
- 函数返回 `ESP_OK`

#### Scenario: 初始化失败处理

**Given** I2C 总线通信异常
**When** 调用 `oled_display_init()`
**Then**
- 记录错误日志
- 函数返回 `ESP_FAIL`
- 后续显示调用安全跳过（不崩溃）

---

### Requirement: 主页面显示

主页面 MUST 显示实时数据、趋势图和状态栏，每 2 秒刷新一次。**天气数据已迁移至远程服务器，本地不再显示。**

#### Scenario: 显示主页面（远程模式）

**Given** 系统运行在 MODE_REMOTE 模式
**And** 传感器数据：
- CO₂: 850 ppm
- 温度: 24.5°C
- 湿度: 58%

**And** 风扇状态: FAN_LOW

**When** 调用 `oled_display_main_page(&sensor, FAN_LOW, MODE_REMOTE)`
**Then** 显示内容为：
```
┌────────────────────────────────┐
│ CO₂: 850ppm [良好]    [REMOTE] │  ← 第1行（大字号16px + 模式徽章）
│ 温度:24.5°C  湿度:58%          │  ← 第2行（6x10字体）
│ ┌──────────────────────────┐   │
│ │     CO₂ 趋势图           │   │  ← 第3-6行（扩展趋势图区域）
│ │  ----1000ppm----         │   │     - 1000ppm 基准线
│ │  ....1500ppm....         │   │     - 1500ppm 告警虚线
│ └──────────────────────────┘   │
│ 风扇:[●低速] 3min  WiFi:●●●    │  ← 第7-8行（合并状态栏）
└────────────────────────────────┘
```

#### Scenario: 显示主页面（本地模式）

**Given** 系统运行在 MODE_LOCAL 模式（网络离线 >30min）
**And** 传感器数据：CO₂ 1200 ppm
**And** 风扇状态: FAN_HIGH

**When** 调用 `oled_display_main_page(&sensor, FAN_HIGH, MODE_LOCAL)`
**Then** 显示内容为：
```
┌────────────────────────────────┐
│ CO₂: 1200ppm [偏高]   [LOCAL]  │  ← 本地模式徽章
│ 温度:24.5°C  湿度:58%          │
│ ┌──────────────────────────┐   │
│ │     CO₂ 趋势图           │   │
│ │  ----1000ppm----         │   │
│ │  ....1500ppm....         │   │
│ └──────────────────────────┘   │
│ 风扇:[●高速] 运行中  WiFi:离线  │  ← 网络断开标识
└────────────────────────────────┘
```

#### Scenario: 显示主页面（预热阶段）

**Given** 系统处于 STATE_PREHEATING 状态（预热剩余 45 秒）
**And** CO₂ 数据不稳定（显示为 `---`）

**When** 调用 `oled_display_main_page(&sensor, FAN_OFF, MODE_REMOTE)`
**Then** 显示内容为：
```
┌────────────────────────────────┐
│ CO₂: --- ppm                   │  ← 数据不可信
│ 温度:24.5°C  湿度:58%          │
│                                │
│      ⏱️ 预热中...              │  ← 大字号提示
│      剩余: 45秒                │
│                                │
│ 风扇: [关闭]                   │
│ 请勿操作                       │
└────────────────────────────────┘
```

---

### Requirement: 趋势图绘制

趋势图 MUST 显示最近 10 小时的 CO₂ 浓度变化（60 个数据点，每 10 分钟一个点）。

#### Scenario: 添加趋势图数据点

**Given** 传感器数据包含 CO₂ 浓度 900 ppm
**When** 调用 `oled_add_history_point(&sensor)`
**Then**
- 从 `sensor->pollutants.co2` 提取 CO₂ 数据点
- 数据点添加到环形缓冲区（容量 60）
- 如果缓冲区已满，覆盖最旧的数据点
- 使用静态环形缓冲区，无动态内存分配

#### Scenario: 绘制趋势图

**Given** 趋势图缓冲区中有 30 个数据点（过去 5 小时）
**When** 调用 `oled_display_main_page(...)`
**Then**
- 在显示区域第 3-6 行绘制折线图（扩展区域）
- X 轴：时间（从右到左，最新数据在右侧）
- Y 轴：CO₂ 浓度（400-2000 ppm，固定范围）
- 绘制 1000ppm 基准线（实线）
- 绘制 1500ppm 告警线（虚线）
- 标记当前值位置（实心点）
- 使用 `u8g2_DrawLine()` 连接相邻数据点

## REMOVED Requirements

### Requirement: 天气数据显示

**Reason**: 天气 API 已迁移至远程服务器，ESP32 本地不再获取和显示天气数据。

**Migration**: 天气信息（PM2.5、AQI）现由远程服务器 UI 展示，本地 OLED 仅显示室内传感器数据。
