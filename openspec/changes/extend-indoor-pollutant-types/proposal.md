# Change: 扩展室内污染物类别

## Why
当前系统仅支持 CO2 一种室内污染物类型。为支持未来多种污染物检测器的接入（颗粒物、挥发性有机物、甲醛），需要扩展数据结构和接口，预留数据通道。

## What Changes
- 新增 `IndoorPollutants` 数据结构，包含 4 类污染物：CO2、颗粒物(PM)、挥发性有机物(VOC)、甲醛(HCHO)
- 修改 `SensorData` 结构，将 `co2` 字段替换为 `IndoorPollutants pollutants` 结构
- 新增手动数据注入接口，用于填写非 CO2 污染物的模拟数据
- **BREAKING**: `SensorData.co2` 字段移至 `SensorData.pollutants.co2`

## Impact
- Affected specs: sensor-integration, decision-algorithm
- Affected code: `main/main.h`, `main/sensors/sensor_manager.c`, `main/algorithm/decision_engine.c`, `main/algorithm/local_mode.c`, `main/ui/oled_display.c`
