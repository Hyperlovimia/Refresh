# 规范：用户界面

## 概述

本规范定义了 OLED 显示模块的接口和行为，包括主页面显示、告警页面显示和趋势图绘制。

---

## ADDED Requirements

### Requirement: OLED 显示初始化

OLED 显示模块 MUST 提供初始化功能，配置 I2C 通信和显示参数。

#### Scenario: 初始化 OLED 显示

**Given** ESP32-S3 GPIO21/22 已配置为 I2C SDA/SCL（与 SHT35 共享）
**When** 调用 `oled_display_init()`
**Then**
- 初始化 u8g2 库
- 配置 I2C 地址为 `0x3C`
- 设置字体为 u8g2_font_6x10_tf（6x10 像素）
- 清空显示缓冲区
- 函数返回 `ESP_OK`

---

### Requirement: 主页面显示

主页面 MUST 显示实时数据、趋势图和状态栏，每 2 秒刷新一次。

#### Scenario: 显示主页面（正常模式）

**Given** 系统运行在 MODE_NORMAL 模式
**And** 传感器数据：
- CO₂: 850 ppm
- 温度: 24.5°C
- 湿度: 58%

**And** 天气数据：
- PM2.5: 45 μg/m³
- AQI: 优

**And** 风扇状态: FAN_LOW

**When** 调用 `oled_display_main_page(&sensor, &weather, FAN_LOW, MODE_NORMAL)`
**Then** 显示内容为：
```
┌────────────────────┐
│ CO₂: 850ppm [良好] │  ← 第1行（大字号，16px）
│ 温度: 24.5°C  湿度: 58% │  ← 第2行（小字号，10px）
│ 室外: AQI 45 优    │  ← 第3行
│                    │
│ [趋势图区域]        │  ← 第4-6行（48个点的折线图）
│                    │
│ 风扇:[●低速] 3min  │  ← 第7行（状态栏）
│ WiFi:●●●           │  ← 第8行（网络状态）
└────────────────────┘
```

#### Scenario: 显示主页面（本地模式）

**Given** 系统运行在 MODE_LOCAL 模式（网络离线 >30min）
**And** 传感器数据：CO₂ 1200 ppm
**And** 风扇状态: FAN_HIGH

**When** 调用 `oled_display_main_page(&sensor, NULL, FAN_HIGH, MODE_LOCAL)`
**Then** 显示内容为：
```
┌────────────────────┐
│ CO₂: 1200ppm [偏高]│
│ 温度: 24.5°C  湿度: 58% │
│ ⚠️ 本地自主模式    │  ← 无天气数据显示
│                    │
│ [趋势图区域]        │
│                    │
│ 风扇:[●高速] 运行中│
│ WiFi: ⚠️ 离线      │  ← 网络断开标识
└────────────────────┘
```

#### Scenario: 显示主页面（预热阶段）

**Given** 系统处于 STATE_PREHEATING 状态（预热剩余 45 秒）
**And** CO₂ 数据不稳定（显示为 `---`）

**When** 调用 `oled_display_main_page(&sensor, &weather, FAN_OFF, MODE_NORMAL)`
**Then** 显示内容为：
```
┌────────────────────┐
│ CO₂: --- ppm       │  ← 数据不可信
│ 温度: 24.5°C  湿度: 58% │
│                    │
│   ⏱️ 预热中...     │  ← 大字号提示
│   剩余: 45秒       │
│                    │
│ 风扇: [关闭]       │
│ 请勿操作            │
└────────────────────┘
```

---

### Requirement: 告警页面显示

告警页面 MUST 在异常状态时自动弹出，3 秒后自动返回主页面。

#### Scenario: 显示 CO₂ 过高告警

**Given** CO₂ 浓度为 1800 ppm（超过 1500 阈值）
**When** 调用 `oled_display_alert("⚠️ CO₂过高")`
**Then** 显示内容为：
```
┌────────────────────┐
│                    │
│   ⚠️ ⚠️ ⚠️         │  ← 闪烁（1Hz）
│                    │
│   CO₂过高！        │  ← 居中大字号
│   1800 ppm         │
│                    │
│  3秒后自动返回      │  ← 倒计时
│                    │
└────────────────────┘
```
- 告警图标以 1Hz 频率闪烁
- 3 秒后自动切换回主页面

#### Scenario: 显示传感器故障告警

**Given** CO₂ 传感器连续 3 次读取失败
**When** 调用 `oled_display_alert("⚠️ 传感器故障")`
**Then** 显示内容为：
```
┌────────────────────┐
│                    │
│   ⚠️ ⚠️ ⚠️         │
│                    │
│  传感器故障！       │
│  风扇已停止         │
│                    │
│ 请检查硬件连接      │
│                    │
└────────────────────┘
```
- 持续显示，直到传感器恢复

---

### Requirement: 趋势图绘制（预留功能）

趋势图 MUST 显示最近 8 小时的 CO₂ 浓度变化（48 个数据点，每 10 分钟一个点）。

#### Scenario: 添加趋势图数据点

**Given** 当前 CO₂ 浓度为 900 ppm
**When** 调用 `oled_add_history_point(900.0f)`
**Then**
- 数据点添加到环形缓冲区（容量 48）
- 如果缓冲区已满，覆盖最旧的数据点

#### Scenario: 绘制趋势图

**Given** 趋势图缓冲区中有 24 个数据点（过去 4 小时）
**When** 调用 `oled_display_main_page(...)`
**Then**
- 在显示区域第 4-6 行绘制折线图
- X 轴：时间（从右到左，最新数据在右侧）
- Y 轴：CO₂ 浓度（400-2000 ppm，自动缩放）
- 标记当前值位置（实心点）

---

## 数据结构定义

```c
// 趋势图历史数据（环形缓冲区）
#define HISTORY_SIZE 48  // 48个点 = 8小时（10分钟/点）

typedef struct {
    float co2_history[HISTORY_SIZE];  // CO₂ 历史数据
    uint8_t head;                     // 环形缓冲区头指针
    uint8_t count;                    // 当前数据点数量
} OledHistoryData;
```

---

## 接口定义

### OLED 显示模块（ui/oled_display.h）

```c
/**
 * @brief 初始化 OLED 显示
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t oled_display_init(void);

/**
 * @brief 显示主页面
 * @param sensor 传感器数据
 * @param weather 天气数据（MODE_LOCAL 模式下可为 NULL）
 * @param fan 风扇状态
 * @param mode 系统运行模式
 */
void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode);

/**
 * @brief 显示告警页面
 * @param message 告警内容（如 "⚠️ CO₂过高"）
 */
void oled_display_alert(const char *message);

/**
 * @brief 添加趋势图数据点（预留功能）
 * @param co2 CO₂ 浓度（ppm）
 */
void oled_add_history_point(float co2);
```

---

## 约束条件

1. **显示分辨率**：
   - 128×64 像素（单色）
   - 字体：u8g2_font_6x10_tf（6x10 像素）
   - 大字号：u8g2_font_ncenB14_tr（14 像素，用于 CO₂ 数值）

2. **刷新频率**：
   - 主页面：2 秒刷新一次（0.5Hz）
   - 告警页面：1 秒刷新一次（闪烁动画）

3. **I2C 总线共享**：
   - 与 SHT35 共享 I2C 总线（GPIO21/22）
   - 必须使用互斥锁保护 I2C 访问
   - I2C 地址：0x3C（7位地址）

4. **趋势图设计**：
   - 容量：48 个数据点（8 小时）
   - 更新频率：10 分钟/点
   - 存储：环形缓冲区（最旧数据被覆盖）

5. **告警规则**：
   - CO₂ > 1500 ppm → 自动弹出告警
   - 传感器故障 → 持续显示告警
   - 网络断开 >30min → 主页面显示"⚠️ 本地自主模式"

---

## 显示布局详细设计

### 主页面布局（128×64 像素）

```
行号  内容                      像素范围
-------------------------------------------
1     CO₂: 850ppm [良好]        0-16px
2     温度: 24.5°C  湿度: 58%   16-26px
3     室外: AQI 45 优           26-36px
4-6   趋势图区域                36-56px
7     风扇:[●低速] 剩余3min     56-64px (左侧)
8     WiFi:●●●                 56-64px (右侧)
```

### CO₂ 质量等级映射

| CO₂ 浓度（ppm） | 等级标签 |
|----------------|---------|
| < 800          | 优秀    |
| 800-1000       | 良好    |
| 1000-1200      | 一般    |
| 1200-1500      | 偏高    |
| > 1500         | 过高    |

### 风扇状态图标

- `FAN_OFF`: `[○关闭]`
- `FAN_LOW`: `[●低速]`
- `FAN_HIGH`: `[●高速]`

### 网络状态图标

- WiFi 连接：`WiFi:●●●`
- WiFi 断开：`WiFi:⚠️ 离线`

---

## 测试要求

1. **单元测试**：
   - 测试 CO₂ 质量等级映射正确性
   - 测试环形缓冲区逻辑（添加、覆盖）

2. **硬件验证**：
   - 验证 OLED 显示内容正确（人工目视检查）
   - 验证刷新频率（使用手机录像慢放）
   - 验证告警页面闪烁动画

3. **集成测试**：
   - 验证 I2C 总线共享（SHT35 + OLED 同时工作，无冲突）
   - 验证不同模式下显示内容切换

---

## 扩展性

未来可扩展的功能：
- 菜单页面（需要物理按钮输入）
- 多页面切换（传感器详情、网络状态、系统信息）
- 彩色 OLED（如 SSD1351）
- 触摸屏支持
