# 核心实现缺口修复 - 任务分解

## 任务概览

本任务清单将修复工作分解为独立、可验证的小步骤，确保每个步骤完成后系统处于可编译、可运行状态。

**原则**：
- 每个任务独立可验证
- 优先修复高危问题
- 每个任务完成后运行 `idf.py build` 验证
- 保持增量提交，便于问题追踪和回滚

---

## 阶段 1：I2C 互斥保护（高优先级）

### Task 1.1：创建 i2c_mutex 并初始化

**目标**：在 `main/main.c` 中声明和创建 `i2c_mutex`

**步骤**：
1. 在 `main/main.c` 第 44 行后添加：
   ```c
   static SemaphoreHandle_t i2c_mutex = NULL;
   ```
2. 在 `system_init()` 函数中（第 348 行后）添加：
   ```c
   i2c_mutex = xSemaphoreCreateMutex();
   ```
3. 更新第 352 行的条件判断：
   ```c
   if (!data_mutex || !i2c_mutex || !system_events || !alert_queue) {
   ```
4. 更新日志输出（可选，保持一致性）

**验证**：
- [ ] `idf.py build` 成功
- [ ] 日志显示 "同步对象创建成功"
- [ ] 使用 `grep -n "i2c_mutex" main/main.c` 确认新增代码

**预期修改**：
- 文件：`main/main.c`
- 行数：+3 行

---

### Task 1.2：导出 i2c_mutex 访问接口

**目标**：提供全局访问 `i2c_mutex` 的安全接口

**步骤**：
1. 在 `main/main.h` 中添加函数声明：
   ```c
   /**
    * @brief 获取 I2C 互斥锁（用于保护 I2C 总线访问）
    * @return I2C 互斥锁句柄
    */
   SemaphoreHandle_t get_i2c_mutex(void);
   ```
2. 在 `main/main.c` 中实现函数（可放在同步对象定义附近）：
   ```c
   SemaphoreHandle_t get_i2c_mutex(void) {
       return i2c_mutex;
   }
   ```

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "get_i2c_mutex" main/main.h main/main.c` 确认声明和实现

**预期修改**：
- 文件：`main/main.h` (+6 行), `main/main.c` (+3 行)

---

### Task 1.3：在传感器管理器中添加 I2C 锁保护

**目标**：在 SHT35 读取时使用 `i2c_mutex` 保护 I2C 访问

**步骤**：
1. 在 `main/sensors/sensor_manager.c` 顶部添加头文件（如果未包含）：
   ```c
   #include "../main.h"  // 获取 get_i2c_mutex()
   ```
2. 在 `sensor_task()` 函数中找到 SHT35 读取代码（搜索 `sht35_read`）
3. 修改为加锁版本：
   ```c
   // 读取 SHT35 温湿度
   SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
   if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
       ret = sht35_read(&temp, &humi);
       xSemaphoreGive(i2c_mutex);

       if (ret == ESP_OK) {
           // 更新数据...（保持原有逻辑）
       } else {
           ESP_LOGW(TAG, "SHT35 读取失败");
           consecutive_failures++;
       }
   } else {
       ESP_LOGE(TAG, "获取 I2C 锁超时");
       consecutive_failures++;
   }
   ```

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "get_i2c_mutex" main/sensors/sensor_manager.c` 确认调用
- [ ] 确认每个 `xSemaphoreTake` 都有对应的 `xSemaphoreGive`

**预期修改**：
- 文件：`main/sensors/sensor_manager.c` (~10 行修改)

---

### Task 1.4：在显示管理器中添加 I2C 锁保护

**目标**：在 OLED 刷新时使用 `i2c_mutex` 保护 I2C 访问

**步骤**：
1. 在 `main/ui/display_manager.c` 顶部添加头文件（如果未包含）：
   ```c
   #include "../main.h"  // 获取 get_i2c_mutex()
   ```
2. 在 `display_task()` 函数中找到 OLED 刷新代码（搜索 `oled_display`）
3. 修改为加锁版本：
   ```c
   // 刷新 OLED 显示
   SemaphoreHandle_t i2c_mutex = get_i2c_mutex();
   if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
       // OLED 刷新逻辑（保持原有 switch-case 结构）
       switch (current_page) {
           case PAGE_MAIN:
               oled_display_main_page(...);
               break;
           // ...
       }
       xSemaphoreGive(i2c_mutex);
   } else {
       ESP_LOGW(TAG, "获取 I2C 锁超时，跳过本次刷新");
   }
   ```

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "get_i2c_mutex" main/ui/display_manager.c` 确认调用
- [ ] 运行时日志无 "获取 I2C 锁超时" 告警（正常情况）

**预期修改**：
- 文件：`main/ui/display_manager.c` (~8 行修改)

---

### Task 1.5：I2C 并发压力测试

**目标**：验证 I2C 锁能有效防止总线冲突

**步骤**：
1. 编译并刷写固件：
   ```bash
   idf.py build
   # 由用户执行 flash
   ```
2. 运行 1 小时，观察日志：
   ```bash
   idf.py -p /dev/ttyACM0 monitor
   ```
3. 检查项：
   - ✅ 无 "I2C 总线错误" 相关日志
   - ✅ 无 "获取 I2C 锁超时" 告警（正常情况）
   - ✅ OLED 显示正常，无花屏
   - ✅ SHT35 数据无异常跳变（温度在合理范围内）

**验证**：
- [ ] 1 小时压力测试通过
- [ ] 截图或日志存档（记录在操作日志中）

**预期输出**：
- `.claude/operations-log.md` 中记录测试结果

---

## 阶段 2：错误恢复流程（高优先级）

### Task 2.1：实现 sensor_manager_reinit() 函数

**目标**：提供传感器重新初始化接口，用于故障恢复

**步骤**：
1. 在 `main/sensors/sensor_manager.h` 中添加声明：
   ```c
   /**
    * @brief 重新初始化传感器管理器（错误恢复时使用）
    */
   esp_err_t sensor_manager_reinit(void);
   ```
2. 在 `main/sensors/sensor_manager.c` 中实现函数：
   ```c
   esp_err_t sensor_manager_reinit(void) {
       ESP_LOGI(TAG, "重新初始化传感器管理器");

       // 清空连续失败计数器
       consecutive_failures = 0;

       // 清空传感器缓冲区
       memset(&cached_sensor_data, 0, sizeof(SensorData));

       // 重新初始化 CO2 传感器
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

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "sensor_manager_reinit" main/sensors/sensor_manager.*` 确认声明和实现

**预期修改**：
- 文件：`main/sensors/sensor_manager.h` (+5 行), `main/sensors/sensor_manager.c` (+25 行)

---

### Task 2.2：修改 handle_error_state() 调用恢复函数

**目标**：在传感器恢复时调用 `sensor_manager_reinit()` 清空状态

**步骤**：
1. 在 `main/main.c` 顶部添加头文件（如果未包含）：
   ```c
   #include "sensors/sensor_manager.h"
   ```
2. 修改 `handle_error_state()` 函数（第 157-161 行）：
   ```c
   if (sensor_manager_is_healthy()) {
       ESP_LOGI(TAG, "传感器恢复，准备重新初始化");

       // 清除故障标志
       xEventGroupClearBits(system_events, EVENT_SENSOR_FAULT);

       // 重新初始化传感器管理器
       esp_err_t ret = sensor_manager_reinit();
       if (ret != ESP_OK) {
           ESP_LOGE(TAG, "传感器重新初始化失败，继续等待");
           return;  // 保持在 ERROR 状态，下次重试
       }

       // 转换到 INIT 状态
       state_transition(STATE_INIT);
   }
   ```

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "sensor_manager_reinit" main/main.c` 确认调用

**预期修改**：
- 文件：`main/main.c` (~8 行修改)

---

### Task 2.3：实现 handle_init_state() 函数

**目标**：使状态机在故障恢复后能重新进入预热流程

**步骤**：
1. 修改 `main/main.c` 中的 `handle_init_state()` 函数（约第 70-73 行）：
   ```c
   /**
    * @brief 处理初始化状态（错误恢复或系统首次启动）
    */
   static void handle_init_state(void) {
       static bool init_completed = false;

       // 错误恢复场景：传感器已重新初始化，直接进入预热
       if (init_completed) {
           ESP_LOGI(TAG, "系统恢复中，重新启动预热流程");
           state_transition(STATE_PREHEATING);
           init_completed = false;  // 重置标志
           return;
       }

       // 首次启动场景：等待 system_init() 完成
       if (xEventGroupWaitBits(system_events, EVENT_SENSOR_READY,
                               pdFALSE, pdFALSE, 0) & EVENT_SENSOR_READY) {
           init_completed = true;
           ESP_LOGI(TAG, "系统初始化完成，进入预热阶段");
           state_transition(STATE_PREHEATING);
       }
   }
   ```

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "handle_init_state" main/main.c` 确认实现

**预期修改**：
- 文件：`main/main.c` (~18 行新增)

---

### Task 2.4：故障恢复流程测试

**目标**：验证传感器故障后能自动恢复并重新进入正常运行状态

**步骤**：
1. 编译并刷写固件（由用户执行）
2. **模拟传感器故障**：
   - 物理断开 SHT35 传感器连接（或短接 SDA/SCL 引脚模拟总线故障）
   - 观察日志：系统应在 3 次失败后进入 `STATE_ERROR`
   - 确认风扇关闭，OLED 显示 "传感器故障"
3. **模拟传感器恢复**：
   - 重新连接 SHT35
   - 观察日志：
     - ✅ "传感器恢复，准备重新初始化"
     - ✅ "传感器管理器重新初始化成功"
     - ✅ "系统恢复中，重新启动预热流程"
     - ✅ 状态转换：`INIT` → `PREHEATING` → `STABILIZING` → `RUNNING`
4. **验证决策任务恢复**：
   - 等待约 5 分钟（60s 预热 + 240s 稳定）
   - 确认风扇能根据 CO₂ 浓度正常启停

**验证**：
- [ ] 故障恢复流程完整执行
- [ ] 状态机重新进入 `STATE_RUNNING`
- [ ] 风扇决策功能恢复正常
- [ ] 截图或日志存档（记录在操作日志中）

**预期输出**：
- `.claude/operations-log.md` 中记录测试结果和时序

---

## 阶段 3：MQTT 发布节拍（中优先级）

### Task 3.1：拆分网络任务定时逻辑

**目标**：使 MQTT 状态每 30 秒上报一次，天气每 10 分钟拉取一次

**步骤**：
1. 修改 `main/main.c` 中的 `network_task()` 函数（第 250-284 行）：
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

           // 任务休眠 1 秒（确保及时响应）
           vTaskDelay(pdMS_TO_TICKS(1000));
       }
   }
   ```

**验证**：
- [ ] `idf.py build` 成功
- [ ] 使用 `grep -n "MQTT_PUBLISH_INTERVAL_SEC" main/main.c` 确认常量被使用
- [ ] 代码审查：确认两个定时器独立工作

**预期修改**：
- 文件：`main/main.c` (~30 行修改)

---

### Task 3.2：MQTT 发布频率测试

**目标**：验证 MQTT 状态消息每 30 秒上报一次

**步骤**：
1. 编译并刷写固件（由用户执行）
2. 启用日志时间戳：
   ```bash
   idf.py menuconfig
   # Component config → Log output → Show timestamp → Yes
   idf.py build
   ```
3. 运行 10 分钟，观察日志：
   ```bash
   idf.py -p /dev/ttyACM0 monitor | grep "MQTT"
   ```
4. 检查项：
   - ✅ "MQTT 状态发布" 日志间隔约 30±2 秒
   - ✅ 10 分钟内收到约 20 条状态消息
   - ✅ "拉取天气数据" 日志间隔约 600±5 秒
5. **MQTT 服务器验证**（可选）：
   - 登录 EMQX Cloud 控制台
   - 查看 `home/ventilation/status` 主题的消息历史
   - 确认每分钟收到 2 条消息

**验证**：
- [ ] MQTT 发布频率符合 30 秒要求
- [ ] 天气拉取频率保持 10 分钟不变
- [ ] 截图或日志存档（记录在操作日志中）

**预期输出**：
- `.claude/operations-log.md` 中记录测试结果

---

## 阶段 4：源文件编码修复（低优先级）

### Task 4.1：检测和转换文件编码

**目标**：将受影响文件转换为 UTF-8（无 BOM）格式

**步骤**：
1. 检测当前编码：
   ```bash
   file -i main/sensors/co2_sensor.c
   file -i main/network/wifi_manager.c
   file -i main/ui/oled_display.c
   ```
2. 如果输出显示 `charset=unknown-8bit` 或 `charset=iso-8859-1`，执行转换：
   ```bash
   # 假设原始编码为 GBK
   iconv -f GBK -t UTF-8 main/sensors/co2_sensor.c -o main/sensors/co2_sensor.c.utf8
   mv main/sensors/co2_sensor.c.utf8 main/sensors/co2_sensor.c

   # 重复其他文件
   iconv -f GBK -t UTF-8 main/network/wifi_manager.c -o main/network/wifi_manager.c.utf8
   mv main/network/wifi_manager.c.utf8 main/network/wifi_manager.c

   iconv -f GBK -t UTF-8 main/ui/oled_display.c -o main/ui/oled_display.c.utf8
   mv main/ui/oled_display.c.utf8 main/ui/oled_display.c
   ```
3. 移除 BOM（如果存在）：
   ```bash
   sed -i '1s/^\xEF\xBB\xBF//' main/sensors/co2_sensor.c
   sed -i '1s/^\xEF\xBB\xBF//' main/network/wifi_manager.c
   sed -i '1s/^\xEF\xBB\xBF//' main/ui/oled_display.c
   ```
4. 验证修复：
   ```bash
   cat main/sensors/co2_sensor.c | head -n 30
   # 确认中文注释正常显示
   ```

**验证**：
- [ ] 文件编码为 UTF-8（无 BOM）
- [ ] 中文注释正常显示
- [ ] `idf.py build` 成功
- [ ] `git diff` 确认只修改了编码，逻辑无变化

**预期修改**：
- 文件：`main/sensors/co2_sensor.c`, `main/network/wifi_manager.c`, `main/ui/oled_display.c`（仅编码变更）

---

### Task 4.2：添加 .editorconfig 防止未来编码问题

**目标**：强制项目使用 UTF-8 编码（可选）

**步骤**：
1. 在项目根目录创建 `.editorconfig`：
   ```ini
   # .editorconfig
   root = true

   [*.{c,h,cpp,hpp}]
   charset = utf-8
   end_of_line = lf
   insert_final_newline = true
   trim_trailing_whitespace = true
   ```
2. 提交到版本控制

**验证**：
- [ ] `.editorconfig` 文件存在
- [ ] 配置符合项目规范

**预期修改**：
- 文件：`.editorconfig`（新增）

---

## 阶段 5：集成验证和文档更新

### Task 5.1：完整编译验证

**目标**：确保所有修改能通过编译

**步骤**：
```bash
idf.py fullclean
idf.py build
```

**验证**：
- [ ] 无编译错误
- [ ] 无编译警告
- [ ] 固件大小在合理范围内（<1.5MB）

---

### Task 5.2：规格符合性检查

**目标**：确认所有 Codex 指出的问题得到修复

**步骤**：
1. 对照 `.claude/review-report.md` 中的 4 个问题：
   - [x] 问题 1：I2C 互斥锁已创建并使用
   - [x] 问题 2：错误恢复流程已完善
   - [x] 问题 3：MQTT 发布频率已修正
   - [x] 问题 4：源文件编码已修复
2. 对照 `openspec/specs/system-orchestration/spec.md` 中的场景：
   - [x] 场景：I2C 总线互斥访问（第 212-229 行）
   - [x] 场景：传感器读取连续失败（第 237-246 行）

**验证**：
- [ ] 所有问题得到修复
- [ ] 所有规格场景能通过验证

---

### Task 5.3：更新操作日志

**目标**：记录所有修改和验证结果

**步骤**：
1. 在 `.claude/operations-log.md` 中追加记录：
   - 修改的文件和函数
   - 遇到的问题和解决方法
   - 测试结果截图或日志摘要
2. 格式参考：
   ```markdown
   ## 2025-12-01 修复核心实现缺口

   ### 修改清单
   - `main/main.c`: 新增 i2c_mutex、实现 get_i2c_mutex()、完善 handle_init_state()
   - `main/sensors/sensor_manager.c`: 新增 sensor_manager_reinit()、SHT35 加锁
   - `main/ui/display_manager.c`: OLED 刷新加锁
   - 编码修复：co2_sensor.c, wifi_manager.c, oled_display.c

   ### 测试结果
   - I2C 并发测试：1 小时无错误
   - 故障恢复测试：传感器断开/恢复流程正常
   - MQTT 频率测试：30 秒±2 秒周期符合要求

   ### 问题记录
   - 无
   ```

**验证**：
- [ ] 操作日志完整记录所有修改

---

### Task 5.4：创建 spec 增量文档（可选）

**目标**：如果发现规格本身有遗漏，补充新的场景

**步骤**：
1. 如果规格已完整，本任务可跳过
2. 如果需要补充，创建：
   ```bash
   mkdir -p openspec/changes/fix-core-implementation-gaps/specs/system-orchestration
   ```
3. 编写 `spec.md` 增量（使用 `## MODIFIED Requirements` 格式）

**验证**：
- [ ] 如需增量，文件已创建并符合 OpenSpec 格式
- [ ] 如不需要，标记为"N/A"

---

### Task 5.5：运行 openspec validate 验证提案

**目标**：确保提案文档符合 OpenSpec 规范

**步骤**：
```bash
openspec validate fix-core-implementation-gaps --strict
```

**验证**：
- [ ] 无验证错误
- [ ] 无验证警告

---

## 任务依赖关系

```
阶段 1（I2C 锁）: Task 1.1 → 1.2 → (1.3 并行 1.4) → 1.5
阶段 2（错误恢复）: Task 2.1 → 2.2 → 2.3 → 2.4
阶段 3（MQTT 节拍）: Task 3.1 → 3.2
阶段 4（编码）: Task 4.1 → 4.2（可并行）
阶段 5（验证）: Task 5.1 → 5.2 → 5.3 → 5.4 → 5.5
```

**可并行任务**：
- Task 1.3 和 1.4（两个文件的 I2C 加锁可同时进行）
- Task 4.1 和 4.2（编码转换和配置文件可同时进行）
- 阶段 1-4 可在不同开发分支中并行进行（如需加速）

---

## 回滚计划

如果任何任务失败或引入新问题：

1. **单任务回滚**：
   ```bash
   git checkout HEAD -- <file>
   idf.py build
   ```

2. **阶段回滚**：
   ```bash
   git revert <commit-hash>
   ```

3. **完全回滚**：
   刷写上一个稳定版本固件（由用户执行）

---

## 完成标准

所有任务的验证项都勾选 `[x]`，且满足以下条件：
- ✅ `idf.py build` 无错误和警告
- ✅ 所有 Codex 指出的问题得到修复
- ✅ 所有测试用例通过
- ✅ 操作日志完整记录
- ✅ `openspec validate` 无错误
