# 核心实现缺口修复 - 设计文档

## 概述

本文档描述修复当前实现与规格偏差的技术方案，重点解决 I2C 总线并发保护、错误恢复流程、MQTT 发布节拍和编码规范问题。

## 设计原则

1. **最小侵入性**：仅修改必要部分，不重构现有架构
2. **渐进式验证**：每个修复点独立可验证，避免大爆炸式改动
3. **符合规格**：所有设计严格遵循 `openspec/specs/system-orchestration/spec.md` 定义
4. **保持简单性**：优先使用 FreeRTOS 原生机制，避免引入新依赖

## 问题 1：I2C 总线互斥保护

### 问题分析

**当前状态**：
- SHT35 传感器（传感器任务，2 秒周期）和 OLED 显示器（显示任务，500ms 周期）共享 I2C0 总线
- 两个任务可能同时调用 I2C 驱动函数，导致总线状态机混乱
- ESP-IDF 的 I2C 驱动不是线程安全的（需要上层加锁）

**根因**：
- `main/main.c` 初始化时未创建 `i2c_mutex`
- 各 I2C 设备驱动（SHT35、OLED）未实现互斥保护

### 设计方案

#### 1.1 创建全局 I2C 互斥锁

在 `main/main.c` 中增加：

```c
// 同步对象（第 43-46 行附近）
static SemaphoreHandle_t data_mutex = NULL;
static SemaphoreHandle_t i2c_mutex = NULL;  // 新增
static EventGroupHandle_t system_events = NULL;
static QueueHandle_t alert_queue = NULL;
```

在 `system_init()` 函数中创建（第 347-350 行附近）：

```c
// 创建同步对象
data_mutex = xSemaphoreCreateMutex();
i2c_mutex = xSemaphoreCreateMutex();  // 新增
system_events = xEventGroupCreate();
alert_queue = xQueueCreate(5, sizeof(char) * 64);

if (!data_mutex || !i2c_mutex || !system_events || !alert_queue) {  // 更新条件
    ESP_LOGE(TAG, "同步对象创建失败");
    return ESP_FAIL;
}
```

#### 1.2 导出互斥锁访问接口

在 `main/main.h` 中导出（避免全局变量污染）：

```c
/**
 * @brief 获取 I2C 互斥锁（用于保护 I2C 总线访问）
 * @return I2C 互斥锁句柄
 */
SemaphoreHandle_t get_i2c_mutex(void);
```

在 `main/main.c` 中实现：

```c
SemaphoreHandle_t get_i2c_mutex(void) {
    return i2c_mutex;
}
```

#### 1.3 修改传感器管理器（SHT35 读取）

在 `main/sensors/sensor_manager.c` 的 `sensor_task()` 中（假设 SHT35 读取在此处）：

```c
// 读取 SHT35 温湿度
SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ret = sht35_read(&temp, &humi);
    xSemaphoreGive(i2c_mutex);

    if (ret == ESP_OK) {
        // 更新数据...
    } else {
        ESP_LOGW(TAG, "SHT35 读取失败");
        consecutive_failures++;
    }
} else {
    ESP_LOGE(TAG, "获取 I2C 锁超时");
    consecutive_failures++;
}
```

#### 1.4 修改显示管理器（OLED 刷新）

在 `main/ui/display_manager.c` 的 `display_task()` 中（假设 OLED 刷新在此处）：

```c
// 刷新 OLED 显示
SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    switch (current_page) {
        case PAGE_MAIN:
            oled_display_main_page(...);
            break;
        case PAGE_ALERT:
            oled_display_alert(...);
            break;
        // ...
    }
    xSemaphoreGive(i2c_mutex);
} else {
    ESP_LOGW(TAG, "获取 I2C 锁超时，跳过本次刷新");
}
```

### 设计权衡

| 选项 | 优点 | 缺点 | 决策 |
|------|------|------|------|
| 全局互斥锁 | 简单，易于理解和维护 | 所有 I2C 设备共享锁，可能降低并发度 | ✅ 采用 |
| 每设备独立锁 | 理论上并发度更高 | 复杂，且 I2C 总线本身就是串行的 | ❌ 不采用 |
| I2C 驱动内部锁 | 对上层透明 | 需要修改第三方库，维护成本高 | ❌ 不采用 |

### 验证方法

1. **编译检查**：`idf.py build` 确认无警告
2. **静态分析**：检查所有 I2C 操作都在锁保护范围内
3. **运行时测试**：
   - 使能 FreeRTOS 栈溢出检测（`CONFIG_FREERTOS_CHECK_STACKOVERFLOW=2`）
   - 观察日志中是否有"获取 I2C 锁超时"告警
   - 长时间运行（>1 小时）确认无 I2C 通信错误

---

## 问题 2：错误恢复流程不完整

### 问题分析

**当前流程**：
1. 传感器连续失败 3 次 → `main_task()` 检测到 `!sensor_manager_is_healthy()`
2. 调用 `state_transition(STATE_ERROR)` 并执行 `handle_error_state()`
3. `handle_error_state()` 每 10 秒检查传感器是否恢复
4. 恢复后调用 `state_transition(STATE_INIT)`
5. **问题**：`handle_init_state()` 为空，状态机停留在 `INIT`，决策任务被阻塞

**缺失部分**：
- 传感器恢复后没有重新执行初始化流程
- 状态机没有重新进入 `PREHEATING` → `STABILIZING` → `RUNNING` 的正常路径

### 设计方案

#### 2.1 实现 `handle_init_state()` 函数

在 `main/main.c` 中实现（当前为空函数，约第 70-73 行）：

```c
/**
 * @brief 处理初始化状态（错误恢复或系统首次启动）
 */
static void handle_init_state(void) {
    static bool init_completed = false;

    // 避免重复初始化
    if (init_completed) {
        ESP_LOGI(TAG, "系统恢复中，重新启动预热流程");
        state_transition(STATE_PREHEATING);
        init_completed = false;  // 重置标志
        return;
    }

    // 首次启动时，等待 system_init() 完成
    // system_init() 在 app_main() 中调用，完成后会设置 init_completed
    if (xEventGroupWaitBits(system_events, EVENT_SENSOR_READY,
                            pdFALSE, pdFALSE, 0) & EVENT_SENSOR_READY) {
        init_completed = true;
        ESP_LOGI(TAG, "系统初始化完成，进入预热阶段");
        state_transition(STATE_PREHEATING);
    }
}
```

#### 2.2 修改 `handle_error_state()` 恢复逻辑

在 `main/main.c` 的 `handle_error_state()` 中（第 157-161 行）：

```c
if (sensor_manager_is_healthy()) {
    ESP_LOGI(TAG, "传感器恢复，准备重新初始化");

    // 清除故障标志
    xEventGroupClearBits(system_events, EVENT_SENSOR_FAULT);

    // 重新初始化传感器管理器（清空缓冲区、重置计数器）
    esp_err_t ret = sensor_manager_reinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器重新初始化失败，继续等待");
        return;  // 保持在 ERROR 状态，下次重试
    }

    // 转换到 INIT 状态，由 handle_init_state() 重启预热流程
    state_transition(STATE_INIT);
}
```

#### 2.3 新增 `sensor_manager_reinit()` 函数

在 `main/sensors/sensor_manager.c` 中新增：

```c
/**
 * @brief 重新初始化传感器管理器（错误恢复时使用）
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t sensor_manager_reinit(void) {
    ESP_LOGI(TAG, "重新初始化传感器管理器");

    // 清空连续失败计数器
    consecutive_failures = 0;

    // 清空传感器缓冲区
    memset(&cached_sensor_data, 0, sizeof(SensorData));

    // 重新初始化 CO2 传感器（可能需要软重启）
    esp_err_t ret = co2_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CO2 传感器重新初始化失败");
        return ESP_FAIL;
    }

    // 重新初始化 SHT35
    ret = sht35_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SHT35 重新初始化失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器管理器重新初始化成功");
    return ESP_OK;
}
```

在 `main/sensors/sensor_manager.h` 中声明：

```c
/**
 * @brief 重新初始化传感器管理器（错误恢复时使用）
 */
esp_err_t sensor_manager_reinit(void);
```

#### 2.4 状态转换时序图

```
[首次启动]
app_main() → system_init() → 设置 EVENT_SENSOR_READY
          ↓
STATE_INIT (handle_init_state 检测到 EVENT_SENSOR_READY)
          ↓
STATE_PREHEATING (60秒) → STATE_STABILIZING (240秒) → STATE_RUNNING

[故障恢复]
STATE_ERROR (handle_error_state 检测到传感器恢复)
          ↓
调用 sensor_manager_reinit() 清空状态
          ↓
STATE_INIT (handle_init_state 检测到 init_completed=true)
          ↓
STATE_PREHEATING (60秒) → STATE_STABILIZING (240秒) → STATE_RUNNING
```

### 设计权衡

| 选项 | 优点 | 缺点 | 决策 |
|------|------|------|------|
| 复用 `system_init()` | 代码复用，逻辑统一 | `system_init()` 包含网络初始化等不需要重复的步骤 | ❌ 不采用 |
| 新增 `sensor_manager_reinit()` | 只重置传感器相关状态，轻量 | 需要新增函数 | ✅ 采用 |
| 直接跳到 `PREHEATING` | 最简单 | 跳过了清空缓冲区等必要步骤，可能残留脏数据 | ❌ 不采用 |

### 验证方法

1. **模拟传感器故障**：
   - 物理断开 SHT35 连接
   - 观察系统进入 `STATE_ERROR` 并关闭风扇
2. **模拟传感器恢复**：
   - 重新连接 SHT35
   - 观察系统进入 `STATE_INIT` → `STATE_PREHEATING`
   - 等待 300 秒后确认进入 `STATE_RUNNING`
3. **检查决策任务**：
   - 确认 `decision_task()` 在恢复后能正常执行风扇决策

---

## 问题 3：MQTT 发布节拍不符合规格

### 问题分析

**当前实现**：
- `network_task()` 仅按 `WEATHER_FETCH_INTERVAL_SEC=600` 循环
- `mqtt_publish_status()` 在同一循环中调用，导致 MQTT 每 10 分钟上报一次
- `MQTT_PUBLISH_INTERVAL_SEC=30` 常量未被使用

**规格要求**：
- 状态消息每 30 秒上报一次（设计文档 `design.md:581-583`）
- 告警消息立即上报（事件触发）

### 设计方案

#### 3.1 拆分网络任务定时逻辑

在 `main/main.c` 的 `network_task()` 中（第 250-284 行）：

```c
static void network_task(void *arg) {
    uint32_t last_weather_fetch = 0;
    uint32_t last_mqtt_publish = 0;

    while (1) {
        uint32_t now = xTaskGetTickCount() / configTICK_RATE_HZ;

        // 检查是否需要拉取天气数据（10 分钟周期）
        if (now - last_weather_fetch >= WEATHER_FETCH_INTERVAL_SEC) {
            if (wifi_manager_is_connected()) {
                ESP_LOGI(TAG, "拉取天气数据");
                esp_err_t ret = weather_api_fetch(&shared_weather_data);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "天气数据更新成功");
                } else {
                    ESP_LOGW(TAG, "天气数据拉取失败，使用缓存");
                }
            }
            last_weather_fetch = now;
        }

        // 检查是否需要发布 MQTT 状态（30 秒周期）
        if (now - last_mqtt_publish >= MQTT_PUBLISH_INTERVAL_SEC) {
            if (wifi_manager_is_connected()) {
                SensorData sensor_data;
                FanState fan_state;

                // 获取共享数据（加锁）
                if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    sensor_data = shared_sensor_data;
                    fan_state = shared_fan_state;
                    xSemaphoreGive(data_mutex);

                    // 发布状态
                    esp_err_t ret = mqtt_publish_status(&sensor_data, fan_state);
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "MQTT 状态发布失败");
                    }
                }
            }
            last_mqtt_publish = now;
        }

        // 任务休眠（使用最小周期，确保及时响应）
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 秒轮询
    }
}
```

#### 3.2 优化方案：使用软件定时器（可选）

如果担心 1 秒轮询浪费 CPU，可使用 FreeRTOS 软件定时器：

```c
// 在 system_init() 中创建定时器
TimerHandle_t mqtt_timer = xTimerCreate(
    "mqtt_publish",
    pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_SEC * 1000),
    pdTRUE,  // 自动重载
    NULL,
    mqtt_publish_timer_callback
);

if (mqtt_timer != NULL) {
    xTimerStart(mqtt_timer, 0);
}

// 定时器回调函数
static void mqtt_publish_timer_callback(TimerHandle_t timer) {
    if (wifi_manager_is_connected()) {
        // 发布 MQTT 状态（同上）
    }
}
```

### 设计权衡

| 选项 | 优点 | 缺点 | 决策 |
|------|------|------|------|
| 双计数器轮询（1秒） | 简单，易于理解 | 1 秒轮询略微浪费 CPU | ✅ 采用（优先简单性） |
| 软件定时器 | CPU 效率高 | 需要额外创建定时器，增加复杂度 | 📝 备选（性能优化阶段） |
| 拆分为两个任务 | 逻辑清晰 | 增加任务数量，消耗更多栈空间 | ❌ 不采用 |

### 验证方法

1. **日志观察**：
   - 启用时间戳日志：`idf.py menuconfig` → `Component config` → `Log output` → `Show timestamp`
   - 观察 "MQTT 状态发布" 日志间隔应为 30±2 秒
   - 观察 "拉取天气数据" 日志间隔应为 600±5 秒
2. **MQTT 服务器验证**：
   - 登录 EMQX Cloud 控制台
   - 查看 `home/ventilation/status` 主题的消息频率
   - 确认每分钟收到 2 条消息

---

## 问题 4：源文件编码错误

### 问题分析

**受影响文件**：
- `main/sensors/co2_sensor.c`
- `main/network/wifi_manager.c`
- `main/ui/oled_display.c`

**问题原因**：
- 文件可能使用了 GBK 或其他编码保存，或包含 BOM 标记
- 在 UTF-8 环境下读取时显示为"???"

### 设计方案

#### 4.1 修复步骤

1. **检查当前编码**：
   ```bash
   file -i main/sensors/co2_sensor.c
   file -i main/network/wifi_manager.c
   file -i main/ui/oled_display.c
   ```

2. **转换为 UTF-8（无 BOM）**：
   ```bash
   # 使用 iconv 转换（如果是 GBK）
   iconv -f GBK -t UTF-8 main/sensors/co2_sensor.c -o main/sensors/co2_sensor.c.utf8
   mv main/sensors/co2_sensor.c.utf8 main/sensors/co2_sensor.c

   # 移除 BOM（如果存在）
   sed -i '1s/^\xEF\xBB\xBF//' main/sensors/co2_sensor.c
   ```

3. **验证修复**：
   ```bash
   # 查看文件内容
   cat main/sensors/co2_sensor.c | head -n 30

   # 确认编码为 UTF-8
   file -i main/sensors/co2_sensor.c
   # 应输出：charset=utf-8
   ```

#### 4.2 预防措施

在 `.editorconfig` 中强制编码规范（如果项目未配置）：

```ini
# .editorconfig
root = true

[*.{c,h,cpp,hpp}]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
```

### 验证方法

1. **编译检查**：`idf.py build` 确认无编码警告
2. **文本查看**：使用 `cat` 或 `less` 查看文件，确认中文注释正常显示
3. **Git 对比**：`git diff` 确认只修改了编码，内容逻辑无变化

---

## 集成验证计划

完成所有修复后，执行以下集成测试：

### 阶段 1：编译验证
```bash
idf.py fullclean
idf.py build
```
- ✅ 无编译错误
- ✅ 无编译警告

### 阶段 2：静态分析
- ✅ 所有 I2C 操作都在 `i2c_mutex` 保护范围内
- ✅ 所有状态转换路径都有对应的处理函数
- ✅ 所有定时逻辑使用正确的常量

### 阶段 3：功能测试
1. **I2C 并发测试**：
   - 运行 1 小时，观察无 I2C 通信错误
   - 检查 OLED 显示无花屏
   - 检查 SHT35 数据无异常跳变

2. **故障恢复测试**：
   - 断开 SHT35 → 等待进入 `STATE_ERROR`
   - 重新连接 SHT35 → 确认重新进入 `STATE_RUNNING`（约 5 分钟）
   - 检查风扇在恢复后能正常响应决策

3. **MQTT 节拍测试**：
   - 运行 10 分钟，确认收到 20 条状态消息（每 30 秒一条）
   - 触发告警（CO₂ > 1500ppm），确认立即收到告警消息

### 阶段 4：规格符合性检查
- ✅ 对照 `openspec/specs/system-orchestration/spec.md` 的所有场景
- ✅ 确认每个场景的 Given-When-Then 都能通过

---

## 实施注意事项

1. **锁超时处理**：
   - I2C 锁超时设为 100ms，避免永久阻塞
   - 如果超时，记录错误日志但不崩溃，保持系统可用

2. **状态机健壮性**：
   - 所有状态转换都有对应的处理函数
   - 避免在状态转换过程中持有锁（防止死锁）

3. **时间精度**：
   - 使用 `xTaskGetTickCount()` 而非 `esp_timer_get_time()`，避免溢出问题
   - 容忍 ±2 秒的定时误差（FreeRTOS 调度延迟）

4. **日志级别**：
   - 正常操作使用 `ESP_LOGI`
   - 异常情况使用 `ESP_LOGW`
   - 严重错误使用 `ESP_LOGE`
   - 避免在锁内打印日志（可能导致性能下降）

---

## 回滚方案

如果修复引入新问题，可按以下步骤回滚：

1. **Git 回滚**：
   ```bash
   git revert <commit-hash>
   ```

2. **部分回滚**（如果只有某个修复有问题）：
   - 问题 1（I2C 锁）：注释掉 `xSemaphoreTake/Give` 调用，回到原始实现
   - 问题 2（错误恢复）：在 `handle_error_state()` 中注释掉 `sensor_manager_reinit()` 调用
   - 问题 3（MQTT 节拍）：恢复原始单循环实现
   - 问题 4（编码）：使用 `git checkout -- <file>` 恢复原始文件

3. **应急降级**：
   - 如果系统无法启动，刷写上一个稳定版本固件：
     ```bash
     idf.py -p /dev/ttyACM0 flash
     ```

---

## 文档更新

完成实施后，需要更新以下文档：

1. **操作日志**：`.claude/operations-log.md`
   - 记录修改的文件、函数和理由
   - 记录遇到的问题和解决方法

2. **规格文档**（如有必要）：`openspec/specs/system-orchestration/spec.md`
   - 如果发现规格本身有遗漏或不清晰的地方，补充新的场景

3. **README**（如有必要）：更新编译和测试指南

---

## 总结

本设计文档描述了修复 4 个核心实现缺口的技术方案：

1. **I2C 互斥保护**：创建全局 `i2c_mutex`，在所有 I2C 操作前后加锁
2. **错误恢复流程**：实现 `sensor_manager_reinit()` 和完善 `handle_init_state()`
3. **MQTT 发布节拍**：拆分定时逻辑，使用双计数器轮询
4. **源文件编码**：转换为 UTF-8（无 BOM）格式

所有方案遵循"最小侵入性"和"渐进式验证"原则，确保修复过程可控且可回滚。
