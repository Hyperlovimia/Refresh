# expand-multi-fan-control

## Summary
将系统从单风扇扩展为3风扇独立控制，并修改MQTT协议以支持远程控制器分别控制每个风扇。

## Motivation
当前系统只有1个风扇，无法满足分区通风需求。扩展为3个风扇后，远程控制器可以根据不同区域的空气质量情况，独立控制每个风扇的状态，提供更灵活的通风策略。

## Scope
- **风扇硬件扩展**：从1个风扇扩展到3个风扇，使用独立的LEDC通道和GPIO
- **MQTT协议变更**：命令和状态主题支持3个风扇的独立状态
- **本地模式行为**：网络离线时3个风扇同步动作（等待后续多传感器变更实现分区控制）

## Design Decisions
1. **GPIO分配**：Fan0=GPIO26（保持不变），Fan1=GPIO27，Fan2=GPIO33
2. **LEDC资源**：3个风扇使用同一个LEDC定时器，不同通道（CHANNEL_0/1/2）
3. **命令格式**：扁平JSON格式 `{"fan_0":"HIGH", "fan_1":"LOW", "fan_2":"OFF"}`
4. **状态格式**：扁平JSON格式，与命令格式对称
5. **本地模式**：暂时3个风扇统一根据单个CO2传感器决策，状态同步

## Out of Scope
- 多CO2传感器支持（需独立变更）
- 分区决策算法（依赖多传感器）
- OLED显示界面调整（可后续优化）

## Spec Deltas
- `actuator-control`：多风扇初始化和控制接口
- `network-services`：MQTT命令/状态协议变更

## Impact Analysis
- **API变更**：`fan_control_*` 函数签名增加风扇ID参数
- **数据结构**：FanState需要数组化或引入多风扇状态结构
- **向后兼容**：不保留旧API，直接替换
