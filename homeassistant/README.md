# Home Assistant 集成说明

本目录提供 Home Assistant 的配置文件和蓝图，让你可以：
- 📊 在 HA 仪表板显示设备状态、信号强度
- 📱 收到短信时推送通知到手机
- 📤 通过 HA 远程发送短信
- ⚠️ 设备离线时收到告警

## 准备工作

在开始之前，请确保：

1. ✅ Home Assistant 已安装并正常运行
2. ✅ Home Assistant 已配置好 MQTT 集成（设置 → 设备与服务 → 添加集成 → MQTT）
3. ✅ 短信转发器已配置好 MQTT 并连接成功（网页显示"MQTT 已连接"）
4. ✅ 知道你的设备 ID（在短信转发器网页顶部显示，如 `sms/40a27c/`）

---

## 第一步：添加传感器

这一步让 HA 能够显示短信转发器的状态信息。

### 方法一：直接修改 configuration.yaml（简单）

1. 用文件编辑器打开 Home Assistant 的 `configuration.yaml` 文件
   - 如果用 HA OS：文件编辑器插件 → 浏览到 `/config/configuration.yaml`
   - 如果用 Docker：`/你的配置目录/configuration.yaml`

2. 把 `sensors.yaml` 文件的**全部内容**复制粘贴到 `configuration.yaml` 的末尾

3. 保存文件

4. 重启 Home Assistant：设置 → 系统 → 右上角三个点 → 重启

### 方法二：使用 packages（推荐，配置更整洁）

1. 在 `configuration.yaml` 中添加这一行：
```yaml
homeassistant:
  packages: !include_dir_named packages
```

2. 在 Home Assistant 配置目录下创建 `packages` 文件夹

3. 把 `sensors.yaml` 文件复制到 `packages` 文件夹中

4. 重启 Home Assistant

### 验证传感器是否生效

重启后，进入 **设置 → 设备与服务 → 实体**，搜索"短信"，应该能看到这些实体：

| 实体 | 说明 |
|------|------|
| sensor.短信转发器状态 | online/offline |
| sensor.短信转发器_wifi_信号 | WiFi 信号强度和评价 |
| sensor.短信转发器_4g_信号 | 4G 信号强度和评价 |
| sensor.短信转发器_apn | 当前 APN |
| sensor.短信转发器_ip | 设备 IP 地址 |
| sensor.最近短信发送者 | 最后收到短信的号码 |
| sensor.最近短信内容 | 最后收到的短信内容 |
| binary_sensor.短信转发器在线 | 在线状态开关 |

> ⚠️ 如果显示 unavailable：
> - 等待60秒（设备每60秒上报一次）
> - 检查 MQTT 连接是否正常
> - 检查主题中的设备 ID 是否正确

---

## 第二步：导入蓝图

蓝图让你可以快速创建自动化，不用写代码。

### 手动导入（推荐）

1. 在 Home Assistant 配置目录下，找到或创建 `blueprints/automation` 文件夹
   ```
   config/
   └── blueprints/
       └── automation/
           └── sms_forwarder/    ← 创建这个文件夹
   ```

2. 把 `blueprints/` 下的所有 `.yaml` 文件复制到 `sms_forwarder/` 文件夹

3. 重启 Home Assistant

### 通过 URL 导入

1. 打开 Home Assistant
2. 进入 **设置 → 自动化与场景 → 蓝图**
3. 点击右下角 **导入蓝图**
4. 输入 GitHub 上的蓝图原始链接

---

## 第三步：创建自动化

导入蓝图后，就可以用蓝图快速创建自动化了。

### 示例1：收到短信发通知

1. 进入 **设置 → 自动化与场景 → 自动化**
2. 点击 **创建自动化 → 从蓝图创建**
3. 选择 **短信转发器 - 收到短信通知**
4. 填写配置：
   - **MQTT 主题**：保持默认或修改成你的设备主题
   - **通知服务**：选择你的手机通知服务，比如 `notify.mobile_app_你的手机名`
5. 保存

现在收到短信就会推送到你手机了！

### 示例2：智能验证码提取

这个蓝图会自动识别验证码，通知标题直接显示验证码数字，超方便！

1. 从蓝图创建自动化
2. 选择 **短信转发器 - 验证码提取通知**
3. 填写 MQTT 主题和通知服务
4. 保存

### 示例3：设备离线告警

1. 从蓝图创建自动化
2. 选择 **短信转发器 - 设备离线告警**
3. 配置：
   - **设备状态实体**：选择 `binary_sensor.短信转发器在线`
   - **离线多久后告警**：比如 5 分钟
4. 保存

---

## 第四步：添加仪表板卡片（可选）

在仪表板上显示短信转发器状态。

### 添加实体卡片

1. 进入仪表板编辑模式
2. 添加卡片 → 选择 **实体**
3. 添加这些实体：
   - `binary_sensor.短信转发器在线`
   - `sensor.短信转发器_4g_信号`
   - `sensor.短信转发器_wifi_信号`
   - `sensor.短信转发器_apn`

### 示例 YAML 卡片

```yaml
type: entities
title: 短信转发器
entities:
  - entity: binary_sensor.短信转发器在线
    name: 在线状态
  - entity: sensor.短信转发器_4g_信号
    name: 4G 信号
  - entity: sensor.短信转发器_wifi_信号
    name: WiFi 信号
  - entity: sensor.短信转发器_apn
    name: APN
  - entity: sensor.短信转发器_ip
    name: IP 地址
  - entity: sensor.短信转发器运行时间
    name: 运行时间
```

### 最近短信卡片

```yaml
type: markdown
title: 最近收到的短信
content: |
  **发送者**: {{ states('sensor.最近短信发送者') }}
  
  **内容**: {{ states('sensor.最近短信内容') }}
```

---

## 发送短信

发送短信最简单的方式是直接调用 MQTT 服务。

### 方法一：开发者工具测试

在 **开发者工具 → 服务** 中：

```yaml
service: mqtt.publish
data:
  topic: "sms/40a27c/sms/send"
  payload: '{"phone":"13800138000","message":"测试短信"}'
```

把 `40a27c` 换成你的设备 ID，`13800138000` 换成目标手机号。

### 方法二：直接调用服务

在 **开发者工具 → 服务** 中：

```yaml
service: mqtt.publish
data:
  topic: "sms/40a27c/sms/send"
  payload: '{"phone":"13800138000","message":"测试短信"}'
```

### 方法三：在自动化中发送

比如门铃响了发短信通知：

```yaml
automation:
  - alias: "门铃响发短信"
    trigger:
      - platform: state
        entity_id: binary_sensor.doorbell
        to: "on"
    action:
      - service: mqtt.publish
        data:
          topic: "sms/40a27c/sms/send"
          payload: '{"phone":"13800138000","message":"有人按门铃了！"}'
```

---

## 文件说明

| 文件 | 说明 |
|------|------|
| `sensors.yaml` | MQTT 传感器配置，显示设备状态 |
| `blueprints/sms_notification.yaml` | 收到短信发通知 |
| `blueprints/sms_code_extract.yaml` | 智能提取验证码 |
| `blueprints/offline_alert.yaml` | 设备离线告警 |

---

## 常见问题

### Q: 传感器一直显示 unavailable？

1. 检查 MQTT 服务器连接是否正常
2. 在 MQTT 集成中检查是否能收到消息
3. 确认 `sensors.yaml` 中的主题是否正确（设备 ID 要匹配）
4. 等待60秒（设备每60秒上报一次状态）

### Q: 找不到通知服务？

1. 确保手机上安装了 Home Assistant APP
2. 在 APP 中登录你的 HA
3. 进入 **设置 → 设备与服务 → 设备**，找到你的手机
4. 通知服务名称格式是 `notify.mobile_app_手机名称`

### Q: 收不到短信通知？

1. 确认短信转发器没有开启"仅控制模式"
2. 检查自动化是否启用
3. 在 MQTT 集成中看能否收到 `sms/xxx/sms/received` 主题的消息

### Q: 蓝图导入失败？

1. 确保文件放在正确的位置：`config/blueprints/automation/`
2. 检查 YAML 语法是否正确
3. 重启 Home Assistant

---

## 高级配置

### 多设备支持

如果你有多个短信转发器，复制 `sensors.yaml` 的内容，修改：
- 每个传感器的 `unique_id`（加上设备编号）
- 每个传感器的 `state_topic`（改成对应设备 ID）
- 每个传感器的 `name`（加上设备标识）

### 自定义验证码提取规则

编辑 `sms_code_extract.yaml`，修改 `patterns` 列表添加更多关键词：

```yaml
{% set patterns = ['验证码', '校验码', '动态码', 'code', 'CODE', '码', '密码'] %}
```
