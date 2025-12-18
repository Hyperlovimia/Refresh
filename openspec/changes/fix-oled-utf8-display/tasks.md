## 1. 初始化配置

- [ ] 1.1 在 `oled_display_init()` 中添加 `u8g2_SetFontMode(&g_u8g2, 1)` 启用透明字体模式

## 2. 主页面字体统一

- [ ] 2.1 `draw_co2_value()`: 状态标签字体改为 `u8g2_font_unifont_t_chinese2`，调整坐标
- [ ] 2.2 `draw_mode_badge()`: 字体改为 `u8g2_font_unifont_t_chinese2`，调整坐标
- [ ] 2.3 `draw_temp_humidity()`: 字体改为 `u8g2_font_unifont_t_chinese2`，调整坐标
- [ ] 2.4 `draw_status_bar()`: 字体改为 `u8g2_font_unifont_t_chinese2`，调整坐标

## 3. 告警页面字体统一

- [ ] 3.1 `draw_alert_page()`: 告警消息字体改为 `u8g2_font_unifont_t_chinese2`
- [ ] 3.2 `draw_alert_page()`: 倒计时文本字体改为 `u8g2_font_unifont_t_chinese2`
- [ ] 3.3 调整告警页面布局坐标

## 4. Validation

- [ ] 4.1 编译验证：`idf.py build` 无错误
- [ ] 4.2 功能验证：告警页面中文消息正确显示
- [ ] 4.3 功能验证：主页面所有文本正确显示
- [ ] 4.4 布局验证：各元素位置合理，无重叠
