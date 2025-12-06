## 1. 数据结构修改
- [x] 1.1 在 `main.h` 中定义 `IndoorPollutants` 结构体
- [x] 1.2 修改 `SensorData` 结构，嵌入 `IndoorPollutants`
- [x] 1.3 添加污染物有效性范围常量

## 2. 传感器管理器适配
- [x] 2.1 修改 `sensor_manager_read_all()` 填充新结构
- [x] 2.2 实现 `sensor_manager_set_pollutant()` 手动注入接口

## 3. 决策算法适配
- [x] 3.1 修改 `decision_engine.c` 访问路径 `.co2` → `.pollutants.co2`
- [x] 3.2 修改 `local_mode.c` 访问路径

## 4. 显示与其他模块适配
- [x] 4.1 修改 `oled_display.c` 访问路径

## 5. 验证
- [x] 5.1 编译通过
- [ ] 5.2 功能验证（CO2 读取正常）
