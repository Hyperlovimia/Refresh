# Change: 修复 OLED UTF-8 显示并统一字体

## Why

1. 告警页面使用 `u8g2_font_6x10_tf`（纯 ASCII 字体）渲染告警消息，导致中文字符（如"传感器预热中…"）和 Unicode 字符（如下标 ₂）显示为乱码
2. 主页面与告警页面字体风格不一致，影响视觉统一性
3. 未来若主页面需要中文支持，需再次修改

## What Changes

- **全局字体统一**：将所有文本字体从 `u8g2_font_6x10_tf` / `u8g2_font_5x7_tf` 统一为 `u8g2_font_unifont_t_chinese2`
- **保留数字专用字体**：CO₂ 数值显示保留 `u8g2_font_logisoso16_tn`（大数字更清晰）
- **布局调整**：unifont 为 16×16 像素，需调整各组件 Y 坐标以适应字体高度变化
- **启用 UTF-8 支持**：在初始化时启用透明字体模式

## Impact

- **Affected specs**: `user-interface`
- **Affected code**: `main/ui/oled_display.c`
- **Flash 占用增加**：`u8g2_font_unifont_t_chinese2` 约 19KB（ESP32-S3 Flash 充裕，可接受）
- **视觉变化**：文本字体从 6×10/5×7 统一为 16×16，显示更大更清晰
- **无 API 变更**：所有公共接口保持不变
