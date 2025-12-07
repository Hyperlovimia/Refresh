# expand-multi-fan-control Tasks

## Phase 1: 风扇控制模块重构

### 1.1 数据结构扩展
- [ ] 在 `main.h` 中定义 `FAN_COUNT = 3` 常量
- [ ] 在 `main.h` 中定义 `FanId` 枚举（FAN_0, FAN_1, FAN_2）
- [ ] 在 `main.h` 中定义 `MultiFanState` 结构体（包含3个风扇状态的数组）

**验证**：编译通过

### 1.2 fan_control 模块重构
- [ ] 修改 `fan_control.h` 接口定义：
  - `fan_control_init()` 保持不变（初始化所有3个风扇）
  - `fan_control_set_state(FanId id, FanState state, bool is_night_mode)`
  - `fan_control_set_pwm(FanId id, uint8_t duty)`
  - `fan_control_get_state(FanId id)`
  - `fan_control_get_pwm(FanId id)`
  - `fan_control_set_all(FanState state, bool is_night_mode)` 新增便捷函数
- [ ] 修改 `fan_control.c` 实现：
  - 定义GPIO映射：`FAN_GPIO[3] = {26, 27, 33}`
  - 定义LEDC通道映射：`FAN_CHANNEL[3] = {0, 1, 2}`
  - 修改初始化逻辑配置3个LEDC通道
  - 修改状态数组 `current_state[3]` 和 `current_pwm[3]`
  - 实现各函数的多风扇版本

**验证**：`idf.py build` 编译通过

## Phase 2: MQTT协议变更

### 2.1 命令协议扩展
- [ ] 修改 `parse_remote_command()` 解析新格式：
  ```json
  {"fan_0":"HIGH", "fan_1":"LOW", "fan_2":"OFF"}
  ```
- [ ] 修改 `s_remote_command` 为数组 `s_remote_command[3]`
- [ ] 修改 `mqtt_get_remote_command()` 接口：
  - 新签名：`bool mqtt_get_remote_command(FanState *cmd_array)`
  - 输出3个风扇的命令到数组

**验证**：编译通过

### 2.2 状态上报扩展
- [ ] 修改 `mqtt_publish_status()` 签名：
  - 新签名：`mqtt_publish_status(SensorData *sensor, FanState *fans, SystemMode mode)`
- [ ] 修改JSON构建逻辑，输出格式：
  ```json
  {
    "co2": 850,
    "temp": 24.5,
    "humi": 58,
    "fan_0": "LOW",
    "fan_1": "HIGH",
    "fan_2": "OFF",
    "mode": "REMOTE"
  }
  ```

**验证**：编译通过

## Phase 3: 决策引擎适配

### 3.1 决策接口变更
- [ ] 修改 `decision_make()` 签名：
  - 新签名：`void decision_make(SensorData *sensor, FanState *remote_cmd, SystemMode mode, FanState *out_fans)`
- [ ] 实现逻辑：
  - MODE_REMOTE：直接使用远程命令数组
  - MODE_LOCAL：3个风扇统一根据CO2决策（暂时同步）
  - MODE_SAFE_STOP：全部关闭

**验证**：编译通过

## Phase 4: 系统集成

### 4.1 main.c 适配
- [ ] 修改风扇状态变量为数组 `FanState fan_states[FAN_COUNT]`
- [ ] 修改决策调用和状态上报调用
- [ ] 修改OLED显示调用（如有需要）

**验证**：`idf.py build` 完整编译通过

### 4.2 头文件依赖检查
- [ ] 检查所有依赖 `fan_control.h` 的模块
- [ ] 更新所有调用点

**验证**：编译无警告

## Phase 5: 集成测试

### 5.1 编译验证
- [ ] `idf.py fullclean build` 完整构建通过

### 5.2 功能验证清单（手动）
- [ ] 3个风扇均可独立控制
- [ ] MQTT命令可独立控制每个风扇
- [ ] MQTT状态上报包含3个风扇状态
- [ ] 本地模式下3个风扇同步动作

## Dependencies
- 无外部依赖
- 后续变更（多传感器）将依赖本变更

## Risk Notes
- GPIO33 在某些ESP32-S3模块上可能有限制，需硬件验证
- LEDC通道数量足够（ESP32-S3有8个通道）
