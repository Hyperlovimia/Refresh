# system-orchestration Specification

## Purpose
TBD - created by archiving change implement-air-quality-system. Update Purpose after archive.
## Requirements
### Requirement: 系统初始化

系统启动时 MUST 依次初始化所有模块，任一模块初始化失败时进入错误处理流程。

#### Scenario: 正常初始化流程

**Given** ESP32-S3 上电启动
**When** 执行 `app_main()` 函数
**Then** 依次执行：
1. NVS 初始化（`nvs_flash_init()`）
2. 传感器管理器初始化（`sensor_manager_init()`）
3. 风扇控制初始化（`fan_control_init()`）
4. WiFi 管理器初始化（`wifi_manager_init()`）
5. 天气 API 初始化（`weather_api_init()`）
6. MQTT 客户端初始化（`mqtt_client_init()`）
7. OLED 显示初始化（`oled_display_init()`）
8. 所有模块初始化成功 → 创建 FreeRTOS 任务 → 进入 STATE_PREHEATING 状态

#### Scenario: 传感器初始化失败

**Given** CO₂ 传感器硬件未连接
**When** 调用 `sensor_manager_init()`
**Then**
- 初始化失败，返回 `ESP_FAIL`
- 打印错误日志：`ESP_LOGE("MAIN", "传感器初始化失败")`
- 进入 STATE_ERROR 状态
- 强制设置风扇为 `FAN_OFF`
- 显示告警页面："⚠️ 传感器故障"

#### Scenario: 网络模块初始化失败（非致命）

**Given** WiFi 或 MQTT 初始化失败
**When** 调用 `wifi_manager_init()` 或 `mqtt_client_init()`
**Then**
- 打印警告日志：`ESP_LOGW("MAIN", "网络模块初始化失败，将使用本地模式")`
- 继续初始化其他模块
- 系统运行模式设置为 MODE_LOCAL

---

### Requirement: 系统状态机

系统 MUST 实现状态机管理，处理从初始化到运行的状态转换。

#### Scenario: 状态转换序列（正常流程）

**Given** 所有模块初始化成功
**When** 系统启动
**Then** 状态转换序列为：
1. `STATE_INIT`（初始化）
   - 持续时间：<1秒
   - 下一状态：`STATE_PREHEATING`

2. `STATE_PREHEATING`（预热）
   - 持续时间：60秒
   - 行为：
     - CO₂ 数据标记为"预热中"（不可信）
     - 风扇保持关闭
     - OLED 显示倒计时
   - 下一状态：`STATE_STABILIZING`

3. `STATE_STABILIZING`（稳定）
   - 持续时间：240秒
   - 行为：
     - CO₂ 数据标记为"稳定中"
     - 允许决策但降低阈值（CO₂ > 1200 才启动风扇）
     - OLED 显示倒计时
   - 下一状态：`STATE_RUNNING`

4. `STATE_RUNNING`（正常运行）
   - 持续时间：无限
   - 行为：
     - 所有功能正常启用
     - 执行完整决策算法
     - 监控传感器健康状态
   - 异常转换：传感器故障 → `STATE_ERROR`

#### Scenario: 传感器故障导致进入错误状态

**Given** 系统运行在 `STATE_RUNNING` 状态
**When** CO₂ 传感器连续 3 次读取失败
**Then**
- 状态转换为 `STATE_ERROR`
- 调用 `fan_control_set_state(FAN_OFF)` 关闭风扇
- 调用 `oled_display_alert("⚠️ 传感器故障")`
- 每 10 秒检查一次传感器状态
- 传感器恢复 → 返回 `STATE_INIT` 重新初始化

#### Scenario: 状态机循环执行

**Given** 系统运行在任意状态
**When** 主任务循环执行
**Then**
- 每 1 秒检查一次传感器健康状态
- 根据当前状态执行对应的处理函数：
  - `STATE_PREHEATING` → `handle_preheating_state()`
  - `STATE_STABILIZING` → `handle_stabilizing_state()`
  - `STATE_RUNNING` → `handle_running_state()`
  - `STATE_ERROR` → `handle_error_state()`

---

### Requirement: FreeRTOS 任务调度

系统 MUST 创建多个 FreeRTOS 任务，实现并发的传感器读取、决策、网络通信和显示更新。

#### Scenario: 创建 FreeRTOS 任务

**Given** 系统初始化成功
**When** 调用 `app_main()` 中的任务创建代码
**Then** 依次创建以下任务：
1. `sensor_task`（传感器任务，优先级 3，栈 4KB）
2. `decision_task`（决策任务，优先级 3，栈 4KB）
3. `network_task`（网络任务，优先级 2，栈 8KB）
4. `display_task`（显示任务，优先级 2，栈 4KB）

#### Scenario: 传感器任务执行（1Hz）

**Given** 传感器任务已创建
**When** 任务循环执行
**Then**
- 调用 `sensor_manager_read_all(&data)` 读取传感器数据
- 使用互斥锁保护共享数据：
  ```c
  xSemaphoreTake(data_mutex, portMAX_DELAY);
  memcpy(&shared_sensor_data, &data, sizeof(SensorData));
  xSemaphoreGive(data_mutex);
  ```
- 延时 1 秒（`vTaskDelay(pdMS_TO_TICKS(1000))`）

#### Scenario: 决策任务执行（1Hz）

**Given** 决策任务已创建
**When** 任务循环执行
**Then**
- 使用互斥锁读取共享数据：
  ```c
  xSemaphoreTake(data_mutex, portMAX_DELAY);
  memcpy(&sensor, &shared_sensor_data, sizeof(SensorData));
  memcpy(&weather, &shared_weather_data, sizeof(WeatherData));
  xSemaphoreGive(data_mutex);
  ```
- 检测运行模式：
  ```c
  SystemMode mode = decision_detect_mode(wifi_ok, cache_valid, sensor_ok);
  ```
- 执行决策算法：
  ```c
  FanState new_state = decision_make(&sensor, &weather, mode);
  ```
- 设置风扇状态：
  ```c
  fan_control_set_state(new_state);
  ```
- 延时 1 秒

#### Scenario: 网络任务执行（10分钟周期）

**Given** 网络任务已创建
**When** 任务循环执行
**Then**
- 检查 WiFi 连接状态：
  ```c
  if (wifi_manager_is_connected()) {
      // 尝试获取天气数据
      if (weather_api_fetch(&weather_data) == ESP_OK) {
          // 保存到共享缓冲区
          xSemaphoreTake(data_mutex, portMAX_DELAY);
          memcpy(&shared_weather_data, &weather_data, sizeof(WeatherData));
          xSemaphoreGive(data_mutex);
      }
  }
  ```
- 发布 MQTT 状态（30秒周期或状态变化时）：
  ```c
  mqtt_publish_status(&sensor, fan_state);
  ```
- 延时 10 分钟（`vTaskDelay(pdMS_TO_TICKS(600000))`）

#### Scenario: 显示任务执行（0.5Hz）

**Given** 显示任务已创建
**When** 任务循环执行
**Then**
- 读取共享数据（传感器、天气、风扇状态、系统模式）
- 调用 OLED 显示函数：
  ```c
  oled_display_main_page(&sensor, &weather, fan_state, mode);
  ```
- 延时 2 秒（`vTaskDelay(pdMS_TO_TICKS(2000))`）

---

### Requirement: 同步机制

系统 MUST 使用互斥锁和事件组保护共享资源和同步任务。

**变更说明**：本次修复补全了规格要求但实现中遗漏的 I2C 互斥锁机制。

#### Scenario: 创建互斥锁和事件组

**Given** 系统初始化阶段
**When** 调用 `app_main()`
**Then** 创建：
- `data_mutex`：保护共享数据缓冲区（sensor_data、weather_data、fan_state）
- `i2c_mutex`：保护 I2C 总线访问（SHT35 + OLED）✅ **本次修复：补全实现**
- `system_events`：事件组（WiFi 连接、传感器就绪等事件）

**实施内容**：
- 在 `main/main.c` 中声明 `static SemaphoreHandle_t i2c_mutex`
- 在 `system_init()` 中调用 `xSemaphoreCreateMutex()` 创建锁
- 导出 `get_i2c_mutex()` 接口供其他模块使用

---

#### Scenario: I2C 总线互斥访问

**Given** 传感器任务需要读取 SHT35，显示任务需要更新 OLED
**When** 两个任务同时执行
**Then**
- 传感器任务：✅ **本次修复：在 sensor_manager.c 中添加锁保护**
  ```c
  xSemaphoreTake(i2c_mutex, portMAX_DELAY);
  sht35_read(&temp, &humi);
  xSemaphoreGive(i2c_mutex);
  ```
- 显示任务：✅ **本次修复：在 display_manager.c 中添加锁保护**
  ```c
  xSemaphoreTake(i2c_mutex, portMAX_DELAY);
  oled_display_main_page(...);
  xSemaphoreGive(i2c_mutex);
  ```
- 保证 I2C 操作不冲突

**实施内容**：
- 修改 `main/sensors/sensor_manager.c` 的 SHT35 读取逻辑，增加锁保护
- 修改 `main/ui/display_manager.c` 的 OLED 刷新逻辑，增加锁保护
- 锁超时设为 100ms，避免永久阻塞

---

### Requirement: 错误处理

系统 MUST 处理各种错误情况并采取安全措施，包括传感器故障恢复后的完整重新初始化流程。

**变更说明**：本次修复补全了传感器故障恢复后的重新初始化流程，确保状态机能正确重启。

#### Scenario: 传感器读取连续失败

**Given** 传感器读取连续失败 3 次
**When** `sensor_manager_is_healthy()` 返回 `false`
**Then**
- 主任务检测到传感器故障
- 状态转换为 `STATE_ERROR`
- 调用 `fan_control_set_state(FAN_OFF)` 安全停机
- 每 10 秒重试一次传感器初始化 ✅ **本次修复：实现完整恢复流程**

**实施内容**：
- 在 `handle_error_state()` 中检测传感器恢复后调用 `sensor_manager_reinit()`
- 新增 `sensor_manager_reinit()` 函数，清空缓冲区和计数器，重新初始化传感器
- 实现 `handle_init_state()` 函数，在恢复后重新进入 `STATE_PREHEATING`
- 确保状态机完整路径：`ERROR` → `INIT` → `PREHEATING` → `STABILIZING` → `RUNNING`

---

#### Scenario: 传感器恢复后重新初始化（新增明确说明）

**Given** 系统处于 `STATE_ERROR`，传感器已恢复健康
**When** `handle_error_state()` 检测到 `sensor_manager_is_healthy()` 返回 `true`
**Then**
- 调用 `sensor_manager_reinit()` 清空传感器状态
- 清除 `EVENT_SENSOR_FAULT` 事件标志
- 转换到 `STATE_INIT`
- `handle_init_state()` 检测到恢复场景，立即转换到 `STATE_PREHEATING`
- 经过 60 秒预热和 240 秒稳定后，进入 `STATE_RUNNING`
- 决策任务恢复正常执行

**实施内容**：
- 修改 `main/main.c` 的 `handle_error_state()` 恢复逻辑
- 实现 `handle_init_state()` 区分首次启动和错误恢复场景
- 新增 `main/sensors/sensor_manager.c` 的 `sensor_manager_reinit()` 函数
- 验证状态转换时序和决策任务恢复

---

