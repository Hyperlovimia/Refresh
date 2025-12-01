# 传感器集成规格增量

## MODIFIED Requirements

### Requirement: CO₂ 传感器驱动接口

系统 MUST 从 UART 接收并解析 CO₂ 传感器的主动上报帧，提取有效的浓度值并验证数据范围。

**原规格**（`openspec/specs/sensor-integration/spec.md`）：
- CO₂ 传感器使用 JX-CO2-102-5K，UART 接口（GPIO16/17），主动上报模式

**增量修改**：
明确 UART 帧格式（`  xxxx ppm\r\n`，空格填充，数值右对齐）和数据解析规则（范围：0-5000 ppm，JX-CO2-102-5K 规格）。

#### Scenario: 接收并解析 CO₂ 浓度帧

**Given**：
- CO₂ 传感器已通过 UART（9600 8N1）连接
- `co2_sensor_init()` 已成功初始化 UART 驱动
- 传感器工作在主动上报模式（默认），1秒发送一次

**When**：
- 调用 `co2_sensor_read_ppm()`

**Then**：
- 从 UART 缓冲区读取数据，查找完整帧：`  xxxx ppm\r\n`（示例：1235 ppm → `20 20 31 32 33 35 20 70 70 6d 0D 0A` hex）
- 查找 "ppm" 标记，向前提取数值部分，转换为 float
- 验证数值范围：0-5000 ppm（符合 JX-CO2-102-5K 硬件规格）
- 返回有效的 CO₂ 浓度值

**Acceptance Criteria**：
- ✅ 正常帧（如 `  850 ppm\r\n`）返回 850.0f
- ✅ 边界值（如 `    0 ppm\r\n`、` 5000 ppm\r\n`）正确解析
- ✅ 超出范围值（如 ` 9999 ppm\r\n`）返回 -1.0f 并打印 WARN 日志
- ✅ 无效格式（如 `CO2ABC\r\n`）返回 -1.0f
- ✅ UART 无数据（超时）返回 -1.0f

---

### Requirement: 温湿度传感器驱动接口

系统 MUST 通过 I2C 发送测量命令、读取数据、验证 CRC-8 校验和，并将原始值转换为温湿度物理值。

**原规格**（`openspec/specs/sensor-integration/spec.md`）：
- SHT35 传感器使用 I2C 接口（GPIO21/20），地址 0x44

**增量修改**：
明确 I2C 命令序列（单次测量 0x2C 0x06）、CRC-8 校验（多项式 0x31）和转换公式。

#### Scenario: 初始化 I2C 总线并配置 SHT35

**Given**：
- 系统启动，I2C 总线未初始化

**When**：
- 调用 `sht35_init()`

**Then**：
- 配置 I2C 主机模式：SDA=GPIO21, SCL=GPIO20, 速率 100kHz
- 安装 I2C 驱动
- 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ I2C 驱动初始化成功
- ✅ 可通过 `i2cdetect` 扫描到设备地址 0x44
- ✅ 防止重复初始化（第二次调用返回 `ESP_OK` 但不重新配置）

#### Scenario: 读取温湿度并验证 CRC

**Given**：
- I2C 总线已初始化
- SHT35 传感器已连接

**When**：
- 调用 `sht35_read(&temp, &humi)`（注：调用者需在调用前获取 I2C 互斥锁）

**Then**：
1. 发送单次测量命令（高重复性）：`0x2C 0x06`
2. 延迟 15ms（等待测量完成）
3. 读取 6 字节数据：
   - 字节 0-1：温度原始值（MSB 在前）
   - 字节 2：温度 CRC-8
   - 字节 3-4：湿度原始值（MSB 在前）
   - 字节 5：湿度 CRC-8
4. 验证两个 CRC-8 校验和（多项式：0x31，初值：0xFF）
5. 转换原始值到物理值：
   - 温度 = -45 + 175 * (raw_temp / 65535.0)
   - 湿度 = 100 * (raw_humi / 65535.0)
6. 返回 `ESP_OK`

**Acceptance Criteria**：
- ✅ 温度读数在合理范围（-10°C ~ 60°C）
- ✅ 湿度读数在合理范围（0% ~ 100%）
- ✅ CRC 校验失败时返回 `ESP_ERR_INVALID_CRC` 并打印 WARN 日志
- ✅ I2C 通信超时时返回相应错误码并打印 ERROR 日志

