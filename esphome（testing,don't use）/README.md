# ESPHome 短信转发器使用指南

本文档介绍如何使用 ESPHome 来控制 ESP32C3 + ML307R 短信转发器，并集成到 Home Assistant。

## 📋 目录

- [概述](#概述)
- [文件结构](#文件结构)
- [前置要求](#前置要求)
- [快速开始](#快速开始)
- [配置说明](#配置说明)
- [Home Assistant 集成](#home-assistant-集成)
- [高级功能](#高级功能)
- [常见问题](#常见问题)
- [与原版 Arduino 固件对比](#与原版-arduino-固件对比)

---

## 概述

ESPHome 版本将短信转发器的功能以 ESPHome 组件的形式实现，提供：

- ✅ **原生 Home Assistant 集成** - 自动发现，无需手动配置 MQTT
- ✅ **OTA 更新** - 无线固件更新
- ✅ **声明式配置** - YAML 格式配置，易于理解和修改
- ✅ **Web 界面** - 内置 Web 服务器查看状态
- ✅ **服务调用** - 通过 HA 服务发送短信

### 硬件要求

| 硬件 | 说明 |
|------|------|
| ESP32C3 Super Mini | 主控芯片 |
| ML307R-DC | 4G LTE 模块 |
| 4G 天线 | LTE 天线 |
| SIM 卡 | 移动/联通卡（电信可能不支持）|

### 接线方式

```
ESP32C3          ML307R
─────────        ──────
GPIO3 (TX)  ──→  RX
GPIO4 (RX)  ←──  TX
GND         ───  GND
5V          ───  VCC
5V          ───  EN (使能，必须连接!)
```

> ⚠️ **重要**: ML307R 的 EN 引脚必须接到 5V，否则模块不会启动！

---

## 文件结构

```
esphome/
├── sms_forwarder.yaml          # 完整配置 (带内置脚本)
├── sms_forwarder_simple.yaml   # 精简配置 (使用自定义组件)
├── secrets.yaml.example         # 密钥模板
├── components/                  # 自定义组件
│   └── ml307r_modem/
│       ├── __init__.py         # ESPHome 组件定义
│       ├── ml307r_modem.h      # C++ 头文件
│       └── ml307r_modem.cpp    # C++ 实现
└── README.md                    # 本文档
```

---

## 前置要求

### 1. 安装 ESPHome

**方式一：使用 pip 安装**
```bash
pip install esphome
```

**方式二：使用 Home Assistant 插件**
- 在 HA 的「设置 → 加载项 → 加载项商店」中搜索 `ESPHome`
- 安装并启动

**方式三：使用 Docker**
```bash
docker run -d --name esphome \
  -v /path/to/esphome:/config \
  -p 6052:6052 \
  esphome/esphome
```

### 2. 准备配置文件

```bash
cd esphome

# 复制密钥模板
cp secrets.yaml.example secrets.yaml

# 编辑密钥文件，填入你的真实配置
nano secrets.yaml
```

---

## 快速开始

### 步骤 1: 配置 secrets.yaml

```yaml
# WiFi 配置
wifi_ssid: "你的WiFi名称"
wifi_password: "你的WiFi密码"

# AP 模式密码
ap_password: "smsforwarder"

# API 加密密钥 (生成方法见下方)
api_encryption_key: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# OTA 密码
ota_password: "smsforwarder_ota"

# Web 界面认证
web_username: "admin"
web_password: "admin123"

# MQTT 配置 (可选)
mqtt_broker: "192.168.1.100"
mqtt_port: 1883
mqtt_username: "mqtt_user"
mqtt_password: "mqtt_password"
```

**生成 API 加密密钥：**
```bash
# Python
python3 -c "import secrets; import base64; print(base64.b64encode(secrets.token_bytes(32)).decode())"

# 或使用 OpenSSL
openssl rand -base64 32
```

### 步骤 2: 编译固件

```bash
# 验证配置
esphome config sms_forwarder_simple.yaml

# 编译固件
esphome compile sms_forwarder_simple.yaml
```

### 步骤 3: 烧录固件

**首次烧录 (USB)：**
```bash
esphome run sms_forwarder_simple.yaml
```

**后续更新 (OTA)：**
```bash
esphome run sms_forwarder_simple.yaml --device 192.168.1.xxx
# 或
esphome run sms_forwarder_simple.yaml --device sms-forwarder.local
```

### 步骤 4: 验证运行

1. 设备成功连接 WiFi 后，访问 `http://sms-forwarder.local`
2. 或者查看 ESPHome 日志：
   ```bash
   esphome logs sms_forwarder_simple.yaml
   ```

---

## 配置说明

### 选择配置文件

| 文件 | 特点 | 推荐场景 |
|------|------|----------|
| `sms_forwarder.yaml` | 完整版，使用内置脚本 | 需要更多自定义 |
| `sms_forwarder_simple.yaml` | 精简版，使用自定义组件 | 推荐，更稳定 |

### 主要配置项

```yaml
substitutions:
  device_name: "sms-forwarder"      # 设备名称 (小写，用于 mDNS)
  friendly_name: "短信转发器"        # 友好名称 (显示用)

uart:
  tx_pin: GPIO3                      # 连接 ML307R RX
  rx_pin: GPIO4                      # 连接 ML307R TX
  baud_rate: 115200                  # 波特率

ml307r_modem:
  # 传感器配置
  sms_sender:                        # 短信发送者
    name: "短信发送者"
  sms_content:                       # 短信内容
    name: "短信内容"
  signal_strength:                   # 4G 信号强度
    name: "4G信号强度"
```

### 添加多 WiFi

```yaml
wifi:
  networks:
    - ssid: "主WiFi"
      password: "password1"
    - ssid: "备用WiFi"
      password: "password2"
  ap:
    ssid: "SMS-Forwarder-AP"
    password: !secret ap_password
```

### 启用 MQTT

```yaml
mqtt:
  broker: !secret mqtt_broker
  port: 1883
  username: !secret mqtt_username
  password: !secret mqtt_password
  topic_prefix: sms/${device_name}
```

---

## Home Assistant 集成

### 自动发现

ESPHome 设备会自动被 Home Assistant 发现：

1. 在 HA 中进入「设置 → 设备与服务」
2. 找到新发现的 `sms-forwarder` 设备
3. 点击「配置」，输入 API 加密密钥

### 可用实体

| 实体 | 类型 | 说明 |
|------|------|------|
| `sensor.sms_forwarder_4g_signal_strength` | Sensor | 4G 信号强度 (dBm) |
| `text_sensor.sms_forwarder_sms_sender` | Text Sensor | 最新短信发送者 |
| `text_sensor.sms_forwarder_sms_content` | Text Sensor | 最新短信内容 |
| `text_sensor.sms_forwarder_sms_timestamp` | Text Sensor | 短信时间 |
| `binary_sensor.sms_forwarder_modem_online` | Binary Sensor | 模块在线状态 |
| `button.sms_forwarder_restart` | Button | 重启设备 |
| `button.sms_forwarder_ping` | Button | 手动 Ping |

### 发送短信服务

在 HA 的「开发者工具 → 服务」中调用：

```yaml
service: esphome.sms_forwarder_send_sms
data:
  phone: "13800138000"
  message: "Hello from Home Assistant!"
```

### 自动化示例

**收到验证码时通知：**
```yaml
automation:
  - alias: "短信验证码通知"
    trigger:
      - platform: state
        entity_id: text_sensor.sms_forwarder_sms_content
    condition:
      - condition: template
        value_template: "{{ '验证码' in trigger.to_state.state }}"
    action:
      - service: notify.mobile_app
        data:
          title: "收到验证码"
          message: "{{ states('text_sensor.sms_forwarder_sms_content') }}"
```

**信号弱时告警：**
```yaml
automation:
  - alias: "4G 信号弱告警"
    trigger:
      - platform: numeric_state
        entity_id: sensor.sms_forwarder_4g_signal_strength
        below: -100
        for:
          minutes: 5
    action:
      - service: notify.persistent_notification
        data:
          message: "短信转发器 4G 信号弱，请检查天线"
```

---

## 高级功能

### 自定义 PDU 解码

当前版本使用简化的 PDU 解码。如需完整支持中文短信，可以：

1. 引入 `pdulib` 库（Arduino 版本使用的库）
2. 修改 `ml307r_modem.cpp` 中的 `process_pdu_()` 方法

### 添加推送服务

可以在配置中添加 HTTP 推送：

```yaml
api:
  services:
    - service: send_push
      variables:
        sender: string
        message: string
      then:
        - http_request.send:
            method: POST
            url: "https://api.telegram.org/botXXX/sendMessage"
            json:
              chat_id: "123456"
              text: !lambda 'return sender + ": " + message;'
```

### 定时保号

```yaml
interval:
  - interval: 24h
    then:
      - lambda: id(modem).do_ping();
      - logger.log: "执行定时 Ping 保号"
```

---

## 常见问题

### Q: 模块无响应 / AT 超时

**可能原因：**
1. EN 引脚未连接到 5V
2. RX/TX 接反
3. 波特率不匹配

**解决方法：**
1. 检查 EN 引脚是否连接到 5V
2. 交换 RX/TX 接线
3. 尝试其他波特率（9600, 57600, 115200）

### Q: 收不到短信

**可能原因：**
1. SIM 卡未正确插入
2. 天线未连接
3. 网络未附着

**解决方法：**
1. 重新插入 SIM 卡
2. 确保天线连接牢固
3. 查看日志中的 `+CGATT` 响应

### Q: 中文乱码

**原因：** 当前 PDU 解码实现不完整

**解决方法：** 需要完善 `ml307r_modem.cpp` 中的 UCS-2 解码

### Q: OTA 更新失败

**可能原因：**
1. 设备不在同一网段
2. 防火墙阻止

**解决方法：**
1. 确保电脑和设备在同一网络
2. 检查防火墙设置
3. 使用 USB 重新烧录

---

## 与原版 Arduino 固件对比

| 特性 | Arduino 版 | ESPHome 版 |
|------|------------|------------|
| Home Assistant 集成 | 需要手动配置 MQTT | 原生支持，自动发现 |
| OTA 更新 | 无 | ✅ 支持 |
| Web 配置界面 | ✅ 完整 | ⚠️ 基础 (仅状态查看) |
| 多推送渠道 | ✅ 邮件/Bark/Telegram等 | ⚠️ 需要自定义实现 |
| PDU 解码 | ✅ 完整 (使用 pdulib) | ⚠️ 简化版 |
| 代码维护 | 需要 Arduino IDE | YAML 配置 |
| 多 WiFi | ✅ 支持 | ✅ 支持 |

**建议：**
- 如果主要需要**接入 Home Assistant**，推荐使用 ESPHome 版
- 如果需要**独立运行**和完整的推送功能，推荐使用原版 Arduino 固件

---

## 调试日志

```bash
# 实时查看日志
esphome logs sms_forwarder_simple.yaml

# 增加日志详细程度
logger:
  level: VERY_VERBOSE
```

---

## 更新历史

- **v1.0.0** (2024-12-21)
  - 初始 ESPHome 版本
  - 支持基本短信收发
  - Home Assistant 集成
  - 自定义 ML307R 组件
