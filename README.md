# 低成本短信转发器

> 当前分支为新方案，老方案请前往[luatos分支](https://github.com/chenxuuu/sms_forwarding/tree/old-luatos)。
该项目可能不支持电信卡（CDMA），具体请自测。

[后台页面演示](https://sms.j2.cx/)

本项目旨在使用低成本的硬件设备，实现短信的自动转发功能，支持多种推送方式同时启用。

> 视频教程：[B站视频](https://www.bilibili.com/video/BV1cSmABYEiX)

<img src="assets/photo.png" width="200" />

## 功能

- 支持使用通用AT指令与模块进行通信
- 开启后支持通过WEB界面配置短信转发参数、查询当前状态
- **支持多达5个推送通道同时启用**，每个通道可独立配置
- 支持将收到的短信转发到指定的邮箱
- 支持通过WEB界面主动发送短信，以便消耗余额
- 支持通过WEB界面进行Ping测试，以极低的成本消耗余额
- 支持长短信自动合并（30秒超时）
- 支持管理员短信远程发送短信和重启设备
- **🆕 支持MQTT协议** - 可通过MQTT监测收到的短信、远程发送短信、执行Ping测试

## 推送通道支持

支持以下4种推送方式，可同时启用多个通道：

| 推送方式 | 说明 | 需要配置 |
|---------|------|---------|
| **POST JSON** | 通用HTTP POST | URL |
| **Bark** | iOS推送服务 | Bark服务器URL |
| **GET请求** | URL参数方式 | URL |
| **自定义模板** | 灵活的JSON模板 | URL + 请求体模板 |

### 推送格式说明

- **POST JSON**: `{"sender":"发送者号码","message":"短信内容","timestamp":"时间戳"}`
- **Bark**: `{"title":"发送者号码","body":"短信内容"}`
- **GET请求**: `URL?sender=xxx&message=xxx&timestamp=xxx`（自动URL编码）
- **自定义模板**: 使用`{sender}`、`{message}`、`{timestamp}`占位符

|状态信息|主动ping|
|-|-|
|![](assets/status.png)|![](assets/ping.png)|

## 硬件搭配

- ESP32C3开发板，当前选用[ESP32C3 Super Mini](https://item.taobao.com/item.htm?id=852057780489&skuId=5813710390565)，¥9.5包邮
- ML307R-DC开发板，当前选用[小蓝鲸ML307R-DC核心板](https://item.taobao.com/item.htm?id=797466121802&skuId=5722077108045)，¥16.3包邮
- [4G FPC天线](https://item.taobao.com/item.htm?id=797466121802&skuId=5722077108045)，¥2，与核心板同购

当前成本约¥27.8

## 硬件连接

ESP32C3 与 ML307R-DC 通过串口（UART）连接，接线如下：

```
    ESP32C3 Super Mini       ML307R-DC核心板
  ┌───────────────────┐    ┌─────────────────┐
  │                   │    │─ EN  ───────────│─┐ 
  │       GPIO3 (TX) ─┼───►│ RX              │ │
  │                   │    │                 │ │
  │       GPIO4 (RX) ◄┼────┤ TX              │ │
  │                   │    │                 │ │
  │              GND ─┼────┤ GND             │ │
  │                   │    │                 │ │
  │               5V ─┼────┤ VCC (5V) ───────┼─┘
  │                   │    │                 │
  └───────────────────┘    └─────────────────┘
                           │                 │
                           │  SIM卡槽        │
                           │  (插入Nano SIM) │
                           │                 │
                           │  天线接口       │
                           │  (连接4G天线)   │
                           └─────────────────┘
```

可通过USB连接ESP32C3进行编程和供电，正常工作时，ESP32C3的虚拟串口数据将直接被转发到ML307R-DC，方便调试。

## 软件组成

- ESP32C3运行自己的`Arduino`固件，负责连接WiFi和接收ML307R-DC发送过来的短信数据，然后转发到指定HTTP接口或邮箱
- ML307R-DC运行默认的AT固件，不用动

需要在`Arduino IDE`中单独安装这些库：

- **ReadyMail** by Mobizt
- **pdulib** by David Henry
- **PubSubClient** by Nick O'Leary（MQTT功能需要）

需要在`Arduino IDE`中安装ESP32开发板支持，参考[官方文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)，版型选`MakerGO ESP32 C3 SuperMini`。

## MQTT功能

本项目支持通过MQTT协议进行双向通信，可以实现：
- 📥 **接收短信通知** - 收到短信时自动推送到MQTT
- 📤 **远程发送短信** - 通过MQTT命令发送短信
- 🌐 **Ping测试** - 通过MQTT触发Ping测试
- ⚙️ **设备控制** - 远程重启、查询状态

### MQTT配置

编辑 `code/mqtt_config.h` 文件配置MQTT参数：

```cpp
// 启用MQTT功能（注释掉这行则禁用MQTT）
#define ENABLE_MQTT

// MQTT服务器地址
#define MQTT_SERVER "broker.emqx.io"

// MQTT服务器端口
#define MQTT_PORT 1883

// MQTT认证（可选，留空则不认证）
#define MQTT_USER ""
#define MQTT_PASS ""

// 主题前缀
#define MQTT_TOPIC_PREFIX "sms_forwarding"
```

### MQTT主题

设备会自动使用MAC地址后6位作为设备ID，主题格式为：`{prefix}/{device_id}/...`

#### 发布主题（设备 → MQTT服务器）

| 主题 | 说明 | 消息格式 |
|------|------|----------|
| `{prefix}/{id}/status` | 设备状态 | `{"status":"online/offline","device":"xxx","ip":"xxx"}` |
| `{prefix}/{id}/sms/received` | 收到短信 | `{"sender":"xxx","message":"xxx","timestamp":"xxx","device":"xxx"}` |
| `{prefix}/{id}/sms/sent` | 发送结果 | `{"success":true/false,"phone":"xxx","message":"xxx","device":"xxx"}` |
| `{prefix}/{id}/ping/result` | Ping结果 | `{"success":true/false,"host":"xxx","result":"xxx","device":"xxx"}` |

#### 订阅主题（MQTT服务器 → 设备）

| 主题 | 说明 | 消息格式 |
|------|------|----------|
| `{prefix}/{id}/sms/send` | 发送短信 | `{"phone":"13800138000","message":"短信内容"}` |
| `{prefix}/{id}/ping` | Ping测试 | `{"host":"8.8.8.8"}` 或 `{}` (默认8.8.8.8) |
| `{prefix}/{id}/cmd` | 控制命令 | `{"action":"restart"}` 或 `{"action":"status"}` |

### 使用示例

假设你的设备ID是 `a1b2c3`，主题前缀是 `sms_forwarding`：

**订阅收到的短信：**
```
主题: sms_forwarding/a1b2c3/sms/received
```

**发送短信：**
```
主题: sms_forwarding/a1b2c3/sms/send
消息: {"phone":"13800138000","message":"Hello World!"}
```

**执行Ping测试：**
```
主题: sms_forwarding/a1b2c3/ping
消息: {"host":"baidu.com"}
```

**重启设备：**
```
主题: sms_forwarding/a1b2c3/cmd
消息: {"action":"restart"}
```

### 推荐MQTT客户端

- **MQTTX** - 优秀的跨平台MQTT客户端 (https://mqttx.app/)
- **MQTT.fx** - 经典的MQTT调试工具
- **Home Assistant** - 可集成到智能家居系统
