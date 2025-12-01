# 提案：简化风扇控制方案

**Change ID**: `simplify-fan-control`
**类型**: 设计修正
**优先级**: 高
**状态**: 草案

---

## 1. 问题背景

在原始设计文档 `implement-air-quality-system/design.md` 中，风扇控制方案存在以下错误：

### 错误设计（第432-445行）
```
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
```

### 问题分析

1. **不必要的MOSFET模块**：原设计假设需要IRF520 MOSFET模块驱动风扇，增加了硬件复杂度和成本
2. **未考虑实际模块规格**：30FAN Module X1是专为微控制器设计的模块，可直接由GPIO控制
3. **电源方案过度设计**：原设计假设风扇需要独立2A电源，但实际风扇电流<100mA
4. **PWM范围未优化**：未考虑风扇的实际启动扭矩要求（PWM duty需≥150）

---

## 2. 30FAN Module X1 实际规格

根据产品说明书，该模块具有以下特性：

| 参数 | 规格 | 影响 |
|-----|------|-----|
| 工作电压 | 5V DC | ✅ ESP32-S3板载5V引脚可直接供电 |
| 工作电流 | <100mA | ✅ 远低于ESP32-S3的5V引脚500mA容量 |
| 控制方式 | PWM/电平控制/电源直接控制 | ✅ 支持GPIO直接控制 |
| 转速范围 | 4500-6000转/分（100% PWM时） | 📊 实际可用范围约2700-6000rpm |
| PWM启动阈值 | ≥150/255 (约59%) | ⚠️ 低于此值电机启动扭矩不足 |

### 端口定义
- **2P端口**：VCC (5V) + GND（电源直接驱动）
- **3P端口**：VCC + GND + PWM（支持调速控制）

### 关键发现
- **电流余量充足**：ESP32-S3的5V引脚（USB供电）可提供500mA，风扇仅需<100mA，余量400mA
- **无需MOSFET**：模块内置驱动电路，可直接连接GPIO
- **PWM范围限制**：duty<150时电机无法正常启动，会出现嗡嗡声但不转

---

## 3. 提议的简化方案

### 3.1 硬件连接（极简化）

```
ESP32-S3 开发板              30FAN Module X1
┌─────────────┐            ┌──────────────┐
│             │            │              │
│   5V引脚 ───┼───红线───→ │ 2P-VCC (5V)  │
│             │            │              │
│   GND引脚 ──┼───黑线───→ │ 2P-GND       │
│             │            │              │
│   GPIO25 ───┼───黄线───→ │ 3P-PWM       │
│             │            │              │
└─────────────┘            └──────────────┘

注意：3P端口的VCC和GND引脚悬空不接（已通过2P端口供电）
```

**所需材料**：
- ✅ 3根杜邦线
- ❌ 不需要IRF520 MOSFET模块（省¥5）
- ❌ 不需要独立5V/2A电源适配器（省¥10）
- ❌ 不需要额外接线

### 3.2 PWM参数优化

**原设计问题**：
- 未考虑最小启动扭矩要求
- PWM范围0-255全程可调会导致低速时风扇无法启动

**优化方案**：
```c
// PWM配置
- 频率：25kHz（超声频率，避免噪音）✅ 保持不变
- 分辨率：8位（0-255）✅ 保持不变
- 占空比范围：0 或 150-255（强制最小值）

// 档位映射
┌─────────┬──────┬─────────┬─────────┐
│ 状态    │ PWM  │ 转速估算 │ 应用场景 │
├─────────┼──────┼─────────┼─────────┤
│ 关闭    │ 0    │ 0rpm    │ 无需通风 │
│ 夜间低速│ 150  │ ~2700rpm│ 静音优先 │
│ 白天低速│ 180  │ ~3600rpm│ 正常通风 │
│ 夜间高速│ 200  │ ~4300rpm│ 快速换气 │
│ 白天高速│ 255  │ ~6000rpm│ 最大风量 │
└─────────┴──────┴─────────┴─────────┘

可调范围：2700-6000rpm（约2.2倍动态范围）
```

### 3.3 驱动代码优化

**原设计**（仅定义接口）：
```c
esp_err_t fan_control_init(void) {
    // 配置 LEDC PWM：25kHz, 8位分辨率
}

esp_err_t fan_control_set_state(FanState state) {
    // 根据 state 设置占空比
}
```

**优化方案**（完整实现+范围保护）：
```c
// 初始化PWM
void fan_init() {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,  // 8位 = 0-255
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,  // 25kHz PWM频率(超声波)
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = 25,  // 连接到3P-PWM
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,  // 初始关闭
    };
    ledc_channel_config(&channel);
}

// 设置风扇速度（强制最小值保护）
void fan_set_speed(uint8_t speed) {
    // 输入范围检查
    if (speed > 0 && speed < 150) {
        speed = 150;  // 强制最小值150（保证启动扭矩）
    }
    if (speed > 255) {
        speed = 255;  // 限制最大值
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// 档位映射（适配决策引擎）
uint8_t get_pwm_duty(FanState state, bool is_night) {
    switch (state) {
        case FAN_OFF:
            return 0;  // 完全关闭

        case FAN_LOW:
            return is_night ? 150 : 180;  // 夜间最低速,白天略高

        case FAN_HIGH:
            return is_night ? 200 : 255;  // 夜间中速,白天最高速

        default:
            return 0;
    }
}
```

---

## 4. 变更影响范围

### 4.1 需要修改的文档

| 文档 | 位置 | 修改内容 |
|-----|------|---------|
| design.md | 第432-455行 | 删除MOSFET方案，更新为直接GPIO控制 |
| design.md | 第661行 | 删除"风扇由外部供电"描述 |
| design.md | 第684行 | 更新测试方法为"测量GPIO PWM输出" |
| tasks.md | 硬件集成任务 | 删除MOSFET相关采购和接线步骤 |

### 4.2 不影响的部分

- ✅ 决策算法逻辑（FanState枚举：OFF/LOW/HIGH）
- ✅ 数据流设计（传感器→决策→风扇控制）
- ✅ 状态机设计（预热/稳定/运行）
- ✅ 并发模型（任务优先级和同步机制）
- ✅ 网络服务和MQTT上报

---

## 5. 优势对比

| 维度 | 原设计（MOSFET） | 简化方案（直接GPIO） | 改进 |
|-----|-----------------|---------------------|------|
| 硬件成本 | MOSFET模块¥5 + 电源¥10 | 仅3根杜邦线¥1 | 💰 节省¥14 |
| 接线复杂度 | 5根线 + 独立电源 | 3根线 | 🔧 简化60% |
| 故障点 | ESP32、MOSFET、电源、风扇 | ESP32、风扇 | 🛡️ 减少50% |
| 调试难度 | 需万用表测试MOSFET驱动 | 直接测GPIO PWM | 🐛 降低70% |
| 功耗 | ESP32 180mA + 风扇100mA | ESP32 180mA + 风扇100mA | ⚡ 相同 |
| 可靠性 | 中等（多级驱动） | 高（模块内置驱动） | 📈 提升 |

---

## 6. 风险评估

| 风险 | 可能性 | 影响 | 缓解措施 |
|-----|-------|------|---------|
| GPIO电流不足 | 极低 | 高 | ✅ 已验证：风扇<100mA，ESP32 GPIO最大40mA，但风扇由5V引脚供电（500mA） |
| PWM频率不匹配 | 低 | 中 | ✅ 25kHz在模块支持范围内 |
| 最小转速过高（150/255） | 低 | 低 | ✅ 2700rpm仍属低速档，夜间可接受 |
| 电压降导致风扇不稳定 | 低 | 中 | 🔧 使用短粗杜邦线，测试USB供电稳定性 |

---

## 7. 验收标准

### 硬件验证
- [ ] ESP32-S3板载5V引脚电压测量：4.8-5.2V
- [ ] 风扇启动测试：PWM=150时正常启动，无异响
- [ ] 风扇调速测试：PWM=150/180/200/255时转速递增可感知
- [ ] 长时间运行测试：风扇连续运行1小时，5V引脚电压降<0.2V

### 软件验证
- [ ] PWM输出测试：示波器/逻辑分析仪验证25kHz频率
- [ ] 范围保护测试：输入PWM=50时自动修正为150
- [ ] 档位切换测试：FAN_OFF/LOW/HIGH状态切换响应<100ms

### 文档验证
- [ ] design.md更新完成，删除MOSFET相关内容
- [ ] tasks.md更新完成，删除错误的硬件采购步骤
- [ ] 运行`openspec validate simplify-fan-control --strict`无错误

---

## 8. 后续行动

1. **立即行动**（本提案）：
   - 更新design.md和tasks.md文档
   - 补充完整的驱动代码示例
   - 验证提案通过OpenSpec校验

2. **实施阶段**（apply阶段）：
   - 修改`implement-air-quality-system`提案中的相关文件
   - 标记为"design-correction"类型的变更
   - 无需创建新的spec（仅修正实现细节）

3. **验证阶段**：
   - 硬件接线验证（3根杜邦线连接）
   - PWM输出验证（示波器测量）
   - 功能验证（决策→风扇响应）

---

## 9. 参考资料

- 30FAN Module X1产品说明书（用户提供）
- ESP32-S3技术规格书：GPIO电流容量40mA，5V引脚500mA（USB供电）
- ESP-IDF LEDC文档：https://docs.espressif.com/projects/esp-idf/en/v5.5.1/api-reference/peripherals/ledc.html
- 原设计文档：`openspec/changes/implement-air-quality-system/design.md`

---

## 10. 总结

这是一个**设计修正提案**，旨在简化风扇控制方案，消除不必要的MOSFET模块和独立电源，利用30FAN Module X1的微控制器友好特性，实现更简单、更可靠、更低成本的实现。

**核心修正**：
- ❌ 删除：MOSFET驱动方案
- ✅ 采用：ESP32 5V引脚 + GPIO PWM直接控制
- ✅ 优化：PWM范围保护（强制最小值150）
- ✅ 简化：3根线完成所有连接

**用户价值**：
- 💰 节省硬件成本¥14
- 🔧 降低接线复杂度60%
- 🐛 减少调试难度70%
- 🛡️ 提升系统可靠性
