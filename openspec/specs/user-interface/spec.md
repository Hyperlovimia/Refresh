# user-interface Specification

## Purpose
TBD - created by archiving change implement-air-quality-system. Update Purpose after archive.
## Requirements
### Requirement: OLED 显示初始化

OLED 显示模块 MUST 提供初始化功能，配置 I2C 通信和显示参数。

#### Scenario: 初始化 OLED 显示

**Given** ESP32-S3 GPIO21/22 已配置为 I2C SDA/SCL（与 SHT35 共享）
**When** 调用 `oled_display_init()`
**Then**
- 初始化 u8g2 库（使用 `u8g2_Setup_ssd1306_i2c_128x64_noname_f` 全缓冲模式）
- 配置 I2C 地址为 `0x3C`
- 设置默认文本字体为 `u8g2_font_unifont_t_chinese2`（16×16 像素，支持中文及 ASCII）
- 启用透明字体模式（`u8g2_SetFontMode(&g_u8g2, 1)`）以支持 UTF-8 字符渲染
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

主页面 MUST 显示实时数据、趋势图和状态栏，每 2 秒刷新一次。所有文本 MUST 使用统一的中文字体（`u8g2_font_unifont_t_chinese2`），CO₂ 数值保留大数字字体。

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
│ 850 ppm    OK      [RMT]       │  ← CO₂数值(大字号) + 状态 + 模式徽章
│ T:24.5C H:58%                  │  ← 温湿度（unifont 字体）
│ ┌──────────────────────────┐   │
│ │     CO₂ 趋势图           │   │  ← 趋势图区域
│ │  ----1000ppm----         │   │
│ │  ....1500ppm....         │   │
│ └──────────────────────────┘   │
│ Fan:LOW        WiFi:ON         │  ← 状态栏（unifont 字体）
└────────────────────────────────┘
```
- CO₂ 数值使用 `u8g2_font_logisoso16_tn`（大数字字体）
- 其他文本统一使用 `u8g2_font_unifont_t_chinese2`

#### Scenario: 显示主页面（本地模式）

**Given** 系统运行在 MODE_LOCAL 模式（网络离线 >30min）
**And** 传感器数据：CO₂ 1200 ppm
**And** 风扇状态: FAN_HIGH

**When** 调用 `oled_display_main_page(&sensor, FAN_HIGH, MODE_LOCAL)`
**Then** 显示内容为：
```
┌────────────────────────────────┐
│ 1200 ppm   High    [LOC]       │  ← 本地模式徽章
│ T:24.5C H:58%                  │
│ ┌──────────────────────────┐   │
│ │     CO₂ 趋势图           │   │
│ └──────────────────────────┘   │
│ Fan:HIGH       WiFi:OFF        │  ← 网络断开标识
└────────────────────────────────┘
```

---

### Requirement: 告警页面显示

告警页面 MUST 在异常状态时自动弹出，3 秒后自动返回主页面。告警消息 MUST 支持中文及 Unicode 字符显示，使用统一的中文字体。

#### Scenario: 显示 CO₂ 过高告警

**Given** CO₂ 浓度为 1800 ppm（超过 1500 阈值）
**When** 调用 `oled_display_alert("CO₂浓度过高: 1800 ppm")`
**Then** 显示内容为：
```
┌────────────────────┐
│                    │
│      ! ! !         │  ← 闪烁（1Hz）
│                    │
│ CO₂浓度过高:       │  ← unifont 字体（支持中文）
│ 1800 ppm           │
│                    │
│ 3s auto return     │  ← unifont 字体
│                    │
└────────────────────┘
```
- 告警图标以 1Hz 频率闪烁
- 所有文本使用 `u8g2_font_unifont_t_chinese2` 字体
- 3 秒后自动切换回主页面

#### Scenario: 显示传感器预热告警

**Given** 系统处于 STATE_PREHEATING 状态
**When** 调用 `oled_display_alert("传感器预热中...")`
**Then**
- 中文文本"传感器预热中..."正确显示（无乱码）
- 倒计时文本正常显示
- 3 秒后自动返回

#### Scenario: 显示传感器故障告警

**Given** CO₂ 传感器连续 3 次读取失败
**When** 调用 `oled_display_alert("传感器故障")`
**Then**
- 中文"传感器故障"正确显示
- 所有文本字体风格统一
- 持续显示，直到传感器恢复

### Requirement: 趋势图绘制（预留功能）

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

