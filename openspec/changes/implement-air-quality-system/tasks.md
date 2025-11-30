# 任务清单：实现智能室内空气质量改善系统

## 第一阶段：系统框架和主逻辑（当前实施）

本阶段聚焦于建立完整的代码结构、定义清晰的模块接口、实现 main.c 的系统编排逻辑。各模块仅提供接口定义和桩函数，不实现具体硬件交互功能。

---

### Task 1: 创建项目目录结构和模块文件

**目标**：建立完整的项目目录结构，创建所有模块的源文件和头文件。

**操作**：
```bash
cd /home/hyperlovimia/esp32/Refresh/main
mkdir -p sensors actuators algorithm network ui
touch sensors/co2_sensor.{c,h}
touch sensors/sht35.{c,h}
touch sensors/sensor_manager.{c,h}
touch actuators/fan_control.{c,h}
touch algorithm/decision_engine.{c,h}
touch algorithm/local_mode.{c,h}
touch network/wifi_manager.{c,h}
touch network/weather_api.{c,h}
touch network/mqtt_client.{c,h}
touch ui/oled_display.{c,h}
```

**交付物**：
- 完整的目录结构（sensors/、actuators/、algorithm/、network/、ui/）
- 所有模块的 .c 和 .h 文件（空白或模板内容）

**验证**：
```bash
ls -R main/ | grep -E '\.(c|h)$'
```

**依赖**：无

---

### Task 2: 定义全局数据结构和枚举类型

**目标**：在 main.h 中定义系统级的数据结构、枚举类型和常量。

**数据结构**：
- `SystemMode`（系统运行模式：NORMAL、DEGRADED、LOCAL、SAFE_STOP）
- `SensorData`（传感器数据：CO₂、温度、湿度）
- `WeatherData`（天气数据：PM2.5、温度、风速、时间戳）
- `FanState`（风扇状态：OFF、LOW、HIGH）
- `SystemState`（系统状态：INIT、PREHEATING、STABILIZING、RUNNING、ERROR）

**交付物**：
- `main/main.h` 包含完整的类型定义和常量声明

**验证**：
- 代码编译通过（idf.py build 仅检查语法）

**依赖**：Task 1

---

### Task 3: 定义传感器模块接口

**目标**：在 sensors/ 目录下的头文件中定义传感器驱动的公共接口。

**模块**：
1. **co2_sensor.h**
   - `esp_err_t co2_sensor_init(void)`：初始化 CO₂ 传感器
   - `float co2_sensor_read_ppm(void)`：读取 CO₂ 浓度（ppm）
   - `bool co2_sensor_is_ready(void)`：检查传感器是否就绪
   - `esp_err_t co2_sensor_calibrate(void)`：手动校准（预留）

2. **sht35.h**
   - `esp_err_t sht35_init(void)`：初始化 SHT35
   - `esp_err_t sht35_read(float *temp, float *humi)`：读取温湿度

3. **sensor_manager.h**
   - `esp_err_t sensor_manager_init(void)`：初始化传感器管理器
   - `esp_err_t sensor_manager_read_all(SensorData *data)`：读取所有传感器数据
   - `bool sensor_manager_is_healthy(void)`：检查传感器健康状态

**交付物**：
- sensors/ 目录下各头文件包含完整的函数声明和注释
- sensors/ 目录下各 .c 文件包含桩函数实现（返回 ESP_OK 或模拟数据）

**验证**：
- 头文件可被 main.c 包含
- 桩函数可被调用且返回预期值

**依赖**：Task 2

---

### Task 4: 定义执行器模块接口

**目标**：在 actuators/fan_control.h 中定义风扇控制接口。

**接口**：
- `esp_err_t fan_control_init(void)`：初始化 PWM 和 GPIO
- `esp_err_t fan_control_set_state(FanState state)`：设置风扇状态（OFF/LOW/HIGH）
- `FanState fan_control_get_state(void)`：获取当前风扇状态

**交付物**：
- actuators/fan_control.{c,h} 包含接口定义和桩实现

**验证**：
- 桩函数可正常调用，状态可读写（使用静态变量模拟）

**依赖**：Task 2

---

### Task 5: 定义决策算法模块接口

**目标**：在 algorithm/ 目录下定义决策引擎和本地模式接口。

**模块**：
1. **decision_engine.h**
   - `FanState decision_make(SensorData *sensor, WeatherData *weather, SystemMode mode)`：完整决策算法
   - `SystemMode decision_detect_mode(bool wifi_ok, bool cache_valid, bool sensor_ok)`：运行模式检测

2. **local_mode.h**
   - `FanState local_mode_decide(float co2_ppm)`：本地自主决策（仅基于 CO₂）

**交付物**：
- algorithm/ 目录下各头文件和桩实现

**验证**：
- 桩函数返回固定的 FanState 值（如 FAN_OFF）

**依赖**：Task 2

---

### Task 6: 定义网络服务模块接口

**目标**：在 network/ 目录下定义 WiFi、天气 API、MQTT 客户端接口。

**模块**：
1. **wifi_manager.h**
   - `esp_err_t wifi_manager_init(void)`：初始化 WiFi
   - `esp_err_t wifi_manager_start_provisioning(void)`：启动配网（SmartConfig/BLE）
   - `bool wifi_manager_is_connected(void)`：检查连接状态

2. **weather_api.h**
   - `esp_err_t weather_api_init(void)`：初始化天气 API 客户端
   - `esp_err_t weather_api_fetch(WeatherData *data)`：获取天气数据
   - `void weather_api_get_cached(WeatherData *data)`：获取缓存数据
   - `bool weather_api_is_cache_stale(void)`：检查缓存是否过期

3. **mqtt_client.h**
   - `esp_err_t mqtt_client_init(void)`：初始化 MQTT 客户端
   - `esp_err_t mqtt_publish_status(SensorData *sensor, FanState fan)`：发布状态
   - `esp_err_t mqtt_publish_alert(const char *message)`：发布告警

**交付物**：
- network/ 目录下各头文件和桩实现

**验证**：
- wifi_manager_is_connected() 返回 false（模拟离线）
- weather_api_is_cache_stale() 返回 true（模拟缓存过期）

**依赖**：Task 2

---

### Task 7: 定义用户界面模块接口

**目标**：在 ui/oled_display.h 中定义 OLED 显示接口。

**接口**：
- `esp_err_t oled_display_init(void)`：初始化 OLED
- `void oled_display_main_page(SensorData *sensor, WeatherData *weather, FanState fan, SystemMode mode)`：显示主页面
- `void oled_display_alert(const char *message)`：显示告警页面
- `void oled_add_history_point(float co2)`：添加趋势图数据点（预留）

**交付物**：
- ui/oled_display.{c,h} 包含接口定义和桩实现

**验证**：
- 桩函数可正常调用（不执行实际 I2C 操作）

**依赖**：Task 2

---

### Task 8: 更新 CMakeLists.txt

**目标**：将新创建的模块文件添加到 main 组件的编译配置中。

**操作**：
编辑 `main/CMakeLists.txt`，在 `SRCS` 中添加所有 .c 文件：
```cmake
idf_component_register(
    SRCS
        "main.c"
        "sensors/co2_sensor.c"
        "sensors/sht35.c"
        "sensors/sensor_manager.c"
        "actuators/fan_control.c"
        "algorithm/decision_engine.c"
        "algorithm/local_mode.c"
        "network/wifi_manager.c"
        "network/weather_api.c"
        "network/mqtt_client.c"
        "ui/oled_display.c"
    INCLUDE_DIRS "."
)
```

**交付物**：
- 更新后的 main/CMakeLists.txt

**验证**：
```bash
idf.py build
```
编译通过且无警告

**依赖**：Task 1-7

---

### Task 9: 实现 main.c 系统初始化逻辑

**目标**：在 main.c 中实现系统初始化流程，调用所有模块的 init 函数。

**实现内容**：
1. NVS 初始化（`nvs_flash_init()`）
2. 调用各模块初始化函数：
   - `sensor_manager_init()`
   - `fan_control_init()`
   - `wifi_manager_init()`
   - `weather_api_init()`
   - `mqtt_client_init()`
   - `oled_display_init()`
3. 错误处理：任何模块初始化失败时打印错误并进入安全模式

**交付物**：
- main.c 中的 `system_init()` 函数

**验证**：
- 编译通过
- 可通过单元测试验证错误处理逻辑

**依赖**：Task 3-7

---

### Task 10: 实现 main.c 系统状态机

**目标**：实现系统状态机，管理从初始化到运行的状态转换。

**状态转换**：
```
INIT (初始化)
  ↓ 所有模块初始化成功
PREHEATING (预热，60秒)
  ↓ 预热完成
STABILIZING (稳定，240秒)
  ↓ 稳定完成
RUNNING (正常运行)
  ↓ 传感器故障
ERROR (错误状态，安全停机)
```

**实现内容**：
1. 定义状态变量 `static SystemState current_state`
2. 实现状态转换函数 `void state_transition(SystemState new_state)`
3. 实现状态处理函数：
   - `void handle_init_state()`
   - `void handle_preheating_state()`
   - `void handle_stabilizing_state()`
   - `void handle_running_state()`
   - `void handle_error_state()`
4. 在状态处理函数中更新 OLED 显示（调用 `oled_display_*()` 桩函数）

**交付物**：
- main.c 中的状态机实现

**验证**：
- 单元测试验证状态转换逻辑
- 预热和稳定计时器正确工作

**依赖**：Task 9

---

### Task 11: 创建 FreeRTOS 任务

**目标**：在 main.c 中创建多个 FreeRTOS 任务，实现并发的传感器读取、决策、网络通信和显示更新。

**任务列表**：
1. **传感器任务** (`sensor_task`，优先级 3，1Hz)
   - 调用 `sensor_manager_read_all()` 读取数据
   - 将数据写入共享缓冲区（使用互斥锁保护）

2. **决策任务** (`decision_task`，优先级 3，1Hz)
   - 从共享缓冲区读取传感器和天气数据
   - 调用 `decision_make()` 计算风扇状态
   - 调用 `fan_control_set_state()` 控制风扇

3. **网络任务** (`network_task`，优先级 2，10分钟周期)
   - 检查 WiFi 连接状态
   - 调用 `weather_api_fetch()` 获取天气数据
   - 调用 `mqtt_publish_status()` 上报状态

4. **显示任务** (`display_task`，优先级 2，0.5Hz)
   - 调用 `oled_display_main_page()` 更新显示

5. **主任务** (`app_main`)
   - 执行系统初始化
   - 创建上述任务
   - 运行状态机循环（检查传感器健康状态，处理状态转换）

**实现内容**：
1. 定义共享数据结构和互斥锁
2. 实现各任务函数
3. 在 `app_main()` 中创建任务

**交付物**：
- main.c 中完整的任务实现

**验证**：
- 编译通过
- 任务创建成功（可通过日志验证）

**依赖**：Task 10

---

### Task 12: 添加日志和错误处理

**目标**：在 main.c 和各模块桩函数中添加 ESP_LOG 日志输出，便于调试和验证。

**日志级别**：
- INFO：系统状态转换、模块初始化成功
- WARN：网络断开、缓存过期
- ERROR：模块初始化失败、传感器故障

**实现内容**：
1. 在 main.c 顶部定义 `static const char *TAG = "MAIN"`
2. 在关键位置添加日志：
   - 系统启动：`ESP_LOGI(TAG, "系统启动...")`
   - 状态转换：`ESP_LOGI(TAG, "状态转换: %s -> %s", ...)`
   - 错误：`ESP_LOGE(TAG, "模块初始化失败: %s", ...)`
3. 在各模块桩函数中添加日志

**交付物**：
- 完善的日志输出

**验证**：
- 运行时可通过串口监视器查看日志

**依赖**：Task 11

---

### Task 13: 编写单元测试（可选，建议实施）

**目标**：为 main.c 的状态机逻辑编写单元测试。

**测试内容**：
1. 状态转换逻辑测试
   - 测试正常状态转换序列（INIT → PREHEATING → STABILIZING → RUNNING）
   - 测试错误状态转换（传感器故障导致进入 ERROR 状态）
2. 计时器测试
   - 测试预热计时器（60秒）
   - 测试稳定计时器（240秒）

**实现**：
- 使用 ESP-IDF 的 Unity 测试框架
- 创建 `main/test/test_main.c`

**交付物**：
- 单元测试文件和测试用例

**验证**：
```bash
idf.py test
```

**依赖**：Task 10

---

### Task 14: 编译和验证

**目标**：完整编译项目，确保所有代码无错误和警告。

**操作**：
```bash
cd /home/hyperlovimia/esp32/Refresh
. $HOME/esp/v5.5.1/esp-idf/export.sh
idf.py fullclean
idf.py build
```

**验证标准**：
- [ ] 编译成功，无错误
- [ ] 无编译警告
- [ ] 生成的 .bin 文件大小合理（<1MB）

**交付物**：
- 成功编译的固件（build/Refresh.bin）

**依赖**：Task 1-12

---

## 任务执行顺序

按照依赖关系，建议执行顺序：

```
Task 1 (创建目录结构)
  ↓
Task 2 (定义全局数据结构)
  ↓
Task 3-7 (并行：定义各模块接口)
  ↓
Task 8 (更新 CMakeLists.txt)
  ↓
Task 9 (实现系统初始化)
  ↓
Task 10 (实现状态机)
  ↓
Task 11 (创建 FreeRTOS 任务)
  ↓
Task 12 (添加日志)
  ↓
Task 13 (单元测试，可选)
  ↓
Task 14 (编译验证)
```

**并行任务**：Task 3、4、5、6、7 可以并行执行（定义模块接口）

**关键路径**：Task 1 → Task 2 → Task 8 → Task 9 → Task 10 → Task 11 → Task 14

---

## 验收标准（第一阶段）

- [ ] 所有 14 个任务完成
- [ ] 代码编译通过，无警告
- [ ] main.c 包含完整的系统初始化、状态机、任务调度逻辑
- [ ] 所有模块文件存在，接口定义清晰，桩函数可调用
- [ ] 日志输出完整，便于后续调试
- [ ] （可选）单元测试通过

---

## 后续阶段任务（不在本次范围内）

### 第二阶段：传感器和执行器实现
- 实现 CO₂ 传感器 UART 驱动（主动上报模式解析）
- 实现 SHT35 I2C 驱动
- 实现风扇 PWM 控制
- 硬件验证

### 第三阶段：决策算法实现
- 实现完整决策引擎（Benefit-Cost 模型）
- 实现本地自主决策模式
- 算法单元测试

### 第四阶段：网络服务实现
- 实现 WiFi 配网功能
- 实现和风天气 API 客户端（HTTPS）
- 实现 MQTT 客户端（TLS）
- 网络降级和缓存策略

### 第五阶段：用户界面实现
- 集成 u8g2 库
- 实现 OLED 主页面显示
- 实现趋势图绘制
- 实现告警页面

### 第六阶段：系统集成测试
- 完整功能测试
- 长期稳定性测试（7天×24小时）
- 性能优化
