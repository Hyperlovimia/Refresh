# Change: 实装 OLED 显示驱动模块

## Why

当前 OLED 显示模块 (`main/ui/oled_display.c`) 仅有接口定义和桩实现，所有核心功能均标记为 TODO。需要完成实装以支持：
- 实时显示 CO2、温湿度数据
- 趋势图展示历史数据（扩展区域）
- 告警页面显示

**重要变更：** 天气 API 已迁移至远程服务器，ESP32 本地不再显示天气信息。

## What Changes

- 集成 u8g2 图形库作为 OLED 驱动
- 实现 ESP-IDF I2C HAL 适配层
- **移除 `WeatherData *weather` 参数**（从 `oled_display_main_page` 函数签名）
- 实现 `oled_display_init()` 初始化函数
- 实现 `oled_display_main_page()` 主页面渲染（无天气行，趋势图扩展）
- 实现 `oled_display_alert()` 告警页面渲染
- 实现 `oled_add_history_point()` 历史数据管理
- 添加环形缓冲区存储 48-60 个历史数据点

## Impact

- Affected specs: `user-interface`
- Affected code:
  - `main/ui/oled_display.c` - 核心实现
  - `main/ui/oled_display.h` - **移除 weather 参数，更新函数签名**
  - `main/main.c` - 更新调用点（当前已传 NULL）
  - `main/CMakeLists.txt` - 添加 u8g2 组件依赖
  - 新增 `main/ui/u8g2_esp32_hal.c/h` - I2C HAL 适配层

## Breaking Changes

- **API 变更：** `oled_display_main_page()` 函数签名移除 `WeatherData *weather` 参数
- 旧签名：`void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode);`
- 新签名：`void oled_display_main_page(SensorData *sensor, FanState fan, SystemMode mode);`
