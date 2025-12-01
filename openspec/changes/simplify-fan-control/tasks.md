# 任务清单：风扇控制简化方案

## 1. 文档修正任务

### 任务1.1：修正原始设计文档中的风扇硬件设计

**文件**：`openspec/changes/implement-air-quality-system/design.md`

**位置**：第432-455行（6.4节）

**当前内容**（错误）：
```markdown
### 6.4 风扇控制 (PWM)

**引脚连接**：
- ESP32 GPIO（待选） → MOSFET 栅极 → 风扇正极
- 风扇负极 → GND

**PWM 配置**：
- 频率：25kHz（超声频率，避免噪音）
- 分辨率：8位（0-255）
- 占空比：
  - OFF: 0%
  - LOW: 50%（占空比 128/255）
  - HIGH: 100%（占空比 255/255）

**驱动接口**：
...
```

**修改为**（正确）：
```markdown
### 6.4 风扇控制 (PWM)

**硬件规格**：30FAN Module X1
- 工作电压：5V DC
- 工作电流：<100mA
- 控制方式：PWM调速（3.3V/5V TTL兼容）
- 转速范围：2700-6000 RPM（PWM 59%-100%）

**引脚连接**：
- ESP32 5V引脚 → 风扇模块 2P-VCC
- ESP32 GND → 风扇模块 2P-GND
- ESP32 GPIO25 → 风扇模块 3P-PWM

**PWM 配置**：
- 频率：25kHz（超声频率，避免噪音）
- 分辨率：8位（0-255）
- 占空比范围：0（关闭）或 150-255（运行）
- 档位映射：
  - OFF: 0
  - 夜间低速: 150 (~2700rpm)
  - 白天低速: 180 (~3600rpm)
  - 夜间高速: 200 (~4300rpm)
  - 白天高速: 255 (~6000rpm)

**驱动接口**：
\`\`\`c
esp_err_t fan_control_init(void);
esp_err_t fan_control_set_state(FanState state, bool is_night_mode);
esp_err_t fan_control_set_pwm(uint8_t duty);  // 内部自动clamp到[0]或[150-255]
FanState fan_control_get_state(void);
uint8_t fan_control_get_pwm(void);
\`\`\`
```

**验证**：
- [ ] 删除所有MOSFET相关描述
- [ ] 补充30FAN Module X1的实际规格
- [ ] 更新接线图为3线方案
- [ ] 更新占空比范围为0或150-255
- [ ] 补充完整的驱动接口定义

---

### 任务1.2：修正功耗估算描述

**文件**：`openspec/changes/implement-air-quality-system/design.md`

**位置**：第661行（10.3节）

**当前内容**（错误）：
```markdown
- 风扇（最大功率）：约 2000mA（由外部供电）

**总计**：约 180mA @ 5V（ESP32 相关），风扇独立供电
```

**修改为**（正确）：
```markdown
- 风扇（30FAN模块）：约 100mA @ 5V（由ESP32板载5V引脚供电）

**总计**：约 280mA @ 5V（USB供电，余量220mA）
```

**验证**：
- [ ] 删除"独立供电"描述
- [ ] 更新风扇电流为100mA
- [ ] 更新总功耗为280mA
- [ ] 补充USB供电余量说明

---

### 任务1.3：修正测试方法描述

**文件**：`openspec/changes/implement-air-quality-system/design.md`

**位置**：第684行（11.2节）

**当前内容**（错误）：
```markdown
- 风扇 PWM 控制（万用表测量占空比）
```

**修改为**（正确）：
```markdown
- 风扇 PWM 控制（示波器测量GPIO25输出，验证25kHz频率和占空比）
- 风扇转速测试（PWM=150/180/200/255时转速递增可感知）
- 电源稳定性测试（万用表测量5V引脚电压降<0.2V）
```

**验证**：
- [ ] 删除"万用表测量占空比"（无法直接测量）
- [ ] 补充示波器测试方法
- [ ] 补充转速感知测试
- [ ] 补充电源稳定性测试

---

### 任务1.4：更新原始任务清单

**文件**：`openspec/changes/implement-air-quality-system/tasks.md`

**操作**：查找并删除/修改以下任务
- ❌ 删除："采购IRF520 MOSFET模块"
- ❌ 删除："采购5V/2A独立电源适配器"
- ✅ 修改："风扇接线"任务为"3线接线方案（5V、GND、GPIO25）"
- ✅ 补充："验证ESP32-S3的5V引脚电流容量（500mA）"

**验证**：
- [ ] 所有MOSFET相关任务已删除
- [ ] 接线任务更新为3线方案
- [ ] 补充电源验证任务

---

## 2. 代码实现任务

### 任务2.1：创建风扇控制驱动文件

**文件**：`main/actuators/fan_control.h`

**内容**：
```c
#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// 风扇状态枚举
typedef enum {
    FAN_OFF = 0,
    FAN_LOW = 1,
    FAN_HIGH = 2,
} FanState;

// 初始化风扇控制
esp_err_t fan_control_init(void);

// 设置风扇状态（供决策引擎调用）
esp_err_t fan_control_set_state(FanState state, bool is_night_mode);

// 直接设置PWM占空比（供测试使用）
esp_err_t fan_control_set_pwm(uint8_t duty);

// 获取当前状态
FanState fan_control_get_state(void);

// 获取当前PWM值
uint8_t fan_control_get_pwm(void);

#endif // FAN_CONTROL_H
```

**验证**：
- [ ] 文件创建成功
- [ ] 接口定义与design.md一致
- [ ] 头文件保护正确

---

### 任务2.2：实现风扇控制驱动

**文件**：`main/actuators/fan_control.c`

**实现内容**（参考design.md第3.3节）：
1. 全局变量：`current_fan_state`、`current_pwm_duty`
2. `fan_control_init()`：配置LEDC定时器和通道
3. `fan_clamp_pwm()`：PWM范围保护（0或150-255）
4. `fan_control_set_pwm()`：设置PWM并更新duty
5. `fan_control_get_pwm_duty()`：FanState→PWM映射
6. `fan_control_set_state()`：高层接口，供决策引擎调用
7. `fan_control_get_state()`：获取当前状态
8. `fan_control_get_pwm()`：获取当前PWM值

**验证**：
- [ ] 所有函数实现完成
- [ ] PWM范围保护逻辑正确（0或≥150）
- [ ] 档位映射符合设计（夜间/白天）
- [ ] 日志输出清晰（ESP_LOGI/ESP_LOGD）

---

### 任务2.3：添加CMakeLists.txt配置

**文件**：`main/CMakeLists.txt`

**操作**：
```cmake
# 在SRCS列表中添加
"actuators/fan_control.c"

# 在INCLUDE_DIRS列表中添加
"actuators"
```

**验证**：
- [ ] fan_control.c已添加到编译列表
- [ ] actuators目录已添加到include路径
- [ ] 编译无错误

---

## 3. 集成任务

### 任务3.1：在决策引擎中调用风扇控制

**文件**：`main/algorithm/decision_engine.c`

**修改位置**：决策任务函数

**当前代码**（假设）：
```c
void decision_task(void *pvParameters) {
    FanState new_state = decision_make(...);
    // TODO: 调用风扇控制
}
```

**修改为**：
```c
#include "fan_control.h"

void decision_task(void *pvParameters) {
    SensorData sensor;
    WeatherData weather;
    FanState new_state;
    bool is_night;

    while (1) {
        // 读取数据
        xSemaphoreTake(data_mutex, portMAX_DELAY);
        memcpy(&sensor, &shared_sensor_data, sizeof(SensorData));
        memcpy(&weather, &shared_weather_data, sizeof(WeatherData));
        xSemaphoreGive(data_mutex);

        // 执行决策
        new_state = decision_make(&sensor, &weather, current_mode);

        // 检测夜间模式（22:00-8:00）
        is_night = decision_is_night_mode();

        // 设置风扇状态
        if (new_state != current_fan_state) {
            esp_err_t ret = fan_control_set_state(new_state, is_night);
            if (ret == ESP_OK) {
                current_fan_state = new_state;
                // 上报状态变化（MQTT）
                mqtt_publish_status(&sensor, new_state);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz
    }
}
```

**验证**：
- [ ] 包含fan_control.h头文件
- [ ] 调用fan_control_set_state()
- [ ] 传递is_night_mode参数
- [ ] 处理返回值
- [ ] 状态变化时上报MQTT

---

### 任务3.2：在main.c中初始化风扇控制

**文件**：`main/main.c`

**修改位置**：`app_main()`函数

**当前代码**（假设）：
```c
void app_main() {
    // 初始化传感器
    co2_sensor_init();
    sht35_init();

    // 初始化显示
    oled_display_init();

    // TODO: 初始化风扇控制
}
```

**修改为**：
```c
#include "fan_control.h"

void app_main() {
    esp_err_t ret;

    // 初始化传感器
    ret = co2_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "CO₂传感器初始化失败");
        goto error;
    }

    ret = sht35_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "SHT35初始化失败");
        goto error;
    }

    // 初始化显示
    ret = oled_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "OLED初始化失败");
        goto error;
    }

    // 初始化风扇控制
    ret = fan_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "风扇控制初始化失败");
        goto error;
    }

    // 启动任务
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 3, NULL);
    xTaskCreate(decision_task, "decision", 4096, NULL, 3, NULL);
    // ...

    return;

error:
    system_state = STATE_ERROR;
    fan_control_set_state(FAN_OFF, false);  // 安全停机
}
```

**验证**：
- [ ] 包含fan_control.h头文件
- [ ] 调用fan_control_init()
- [ ] 检查返回值
- [ ] 错误处理中关闭风扇

---

## 4. 测试任务

### 任务4.1：单元测试 - PWM范围保护

**文件**：`main/test/test_fan_control.c`

**测试用例**：
```c
#include "unity.h"
#include "fan_control.h"

// 测试PWM范围保护
void test_pwm_clamping(void) {
    fan_control_init();

    // 测试0（允许）
    fan_control_set_pwm(0);
    TEST_ASSERT_EQUAL_UINT8(0, fan_control_get_pwm());

    // 测试50（自动修正为150）
    fan_control_set_pwm(50);
    TEST_ASSERT_EQUAL_UINT8(150, fan_control_get_pwm());

    // 测试150（保持）
    fan_control_set_pwm(150);
    TEST_ASSERT_EQUAL_UINT8(150, fan_control_get_pwm());

    // 测试255（保持）
    fan_control_set_pwm(255);
    TEST_ASSERT_EQUAL_UINT8(255, fan_control_get_pwm());

    // 测试300（限制为255）
    fan_control_set_pwm(300);
    TEST_ASSERT_EQUAL_UINT8(255, fan_control_get_pwm());
}
```

**验证**：
- [ ] 测试通过
- [ ] 覆盖边界值（0, 1, 149, 150, 255, 256）

---

### 任务4.2：单元测试 - 档位映射

**文件**：`main/test/test_fan_control.c`

**测试用例**：
```c
// 测试档位映射
void test_state_mapping(void) {
    fan_control_init();

    // 白天模式
    fan_control_set_state(FAN_OFF, false);
    TEST_ASSERT_EQUAL_UINT8(0, fan_control_get_pwm());

    fan_control_set_state(FAN_LOW, false);
    TEST_ASSERT_EQUAL_UINT8(180, fan_control_get_pwm());

    fan_control_set_state(FAN_HIGH, false);
    TEST_ASSERT_EQUAL_UINT8(255, fan_control_get_pwm());

    // 夜间模式
    fan_control_set_state(FAN_LOW, true);
    TEST_ASSERT_EQUAL_UINT8(150, fan_control_get_pwm());

    fan_control_set_state(FAN_HIGH, true);
    TEST_ASSERT_EQUAL_UINT8(200, fan_control_get_pwm());
}
```

**验证**：
- [ ] 测试通过
- [ ] 覆盖所有状态（OFF/LOW/HIGH）
- [ ] 覆盖昼夜模式

---

### 任务4.3：硬件测试 - 电源稳定性

**步骤**：
1. 连接万用表到ESP32-S3的5V引脚和GND
2. 记录风扇关闭时电压（V1）
3. 设置PWM=255（最大功率）
4. 记录风扇运行时电压（V2）
5. 计算压降：ΔV = V1 - V2

**验收标准**：
- [ ] V1 ≥ 4.8V
- [ ] ΔV ≤ 0.2V
- [ ] V2 ≥ 4.6V

---

### 任务4.4：硬件测试 - 最小启动扭矩

**步骤**：
1. 设置PWM=0（关闭风扇）
2. 等待风扇完全停止
3. 设置PWM=150
4. 观察风扇启动情况

**验收标准**：
- [ ] 风扇在1秒内启动
- [ ] 无异响（嗡嗡声、卡顿）
- [ ] 转速稳定

---

### 任务4.5：硬件测试 - 调速范围

**步骤**：
1. 依次设置PWM=150, 180, 200, 255
2. 每档位保持10秒
3. 手感/听感判断转速变化

**验收标准**：
- [ ] 每档位转速明显递增
- [ ] 无卡顿或不稳定现象
- [ ] 噪音随转速增加

---

### 任务4.6：集成测试 - 决策联动

**场景1**：CO₂浓度升高
1. 模拟CO₂从800→1200ppm
2. 观察风扇状态：OFF → LOW
3. 验证PWM输出：0 → 150/180

**场景2**：夜间模式切换
1. 风扇状态：HIGH
2. 模拟时间切换到22:00
3. 观察PWM变化：255 → 200

**验收标准**：
- [ ] 状态切换延迟<2秒
- [ ] PWM值符合档位映射
- [ ] 无误触发或抖动

---

## 5. 文档验证任务

### 任务5.1：运行OpenSpec验证

**命令**：
```bash
openspec validate simplify-fan-control --strict
```

**验收标准**：
- [ ] 无语法错误
- [ ] 无结构错误
- [ ] 所有必需文件存在（proposal.md, design.md, tasks.md）

---

### 任务5.2：交叉检查文档一致性

**检查项**：
1. proposal.md、design.md、tasks.md中的接线图一致
2. PWM参数（频率、分辨率、范围）一致
3. 档位映射表一致
4. 驱动接口定义一致

**验收标准**：
- [ ] 所有文档描述一致
- [ ] 无矛盾或冲突
- [ ] 代码实现与设计文档一致

---

## 6. 部署任务

### 任务6.1：更新原始提案

**操作**：
1. 将`simplify-fan-control`的修正内容合并到`implement-air-quality-system`
2. 在`implement-air-quality-system/proposal.md`中添加变更记录

**验证**：
- [ ] 原始提案已更新
- [ ] 变更记录清晰
- [ ] 无残留错误描述

---

### 任务6.2：清理临时文件

**操作**：
1. 确认所有修正已合并到原始提案
2. 归档`simplify-fan-control`到`openspec/changes/archive/`

**验证**：
- [ ] 原始提案包含所有修正
- [ ] 归档完成

---

## 总结

**总任务数**：23
**估算工时**：
- 文档修正：2小时
- 代码实现：4小时
- 集成调试：2小时
- 测试验证：3小时
- 文档验证：1小时

**总计**：约12小时

**关键里程碑**：
1. ✅ 文档修正完成（任务1.1-1.4）
2. ✅ 驱动代码实现（任务2.1-2.3）
3. ✅ 集成完成（任务3.1-3.2）
4. ✅ 测试通过（任务4.1-4.6）
5. ✅ 验证通过（任务5.1-5.2）
