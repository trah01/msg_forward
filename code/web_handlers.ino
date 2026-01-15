/*
 * web_handlers.ino - Web 服务器处理函数实现
 */

// 设置禁止缓存的 HTTP 头
void setNoCacheHeaders() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

// 设置允许缓存的 HTTP 头 (用于静态资源)
void setCacheHeaders(int maxAge) {
  server.sendHeader("Cache-Control", "public, max-age=" + String(maxAge));
}

// 检查 HTTP Basic 认证
bool checkAuth() {
  if (!server.authenticate(config.webUser.c_str(), config.webPass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "SMS Forwarding", "请输入管理员账号密码");
    return false;
  }
  return true;
}

// 处理 CSS 请求 (独立路由，可缓存)
void handleStyleCss() {
  setCacheHeaders(86400);  // 缓存 1 天
  server.send_P(200, "text/css", CSS_CONTENT);
}

// 发送 PROGMEM 字符串并替换占位符
void sendChunkWithReplace(const char* progmemStr, std::function<String(const String&)> replacer) {
  String chunk = FPSTR(progmemStr);
  if (replacer) {
    chunk = replacer(chunk);
  }
  server.sendContent(chunk);
  yield();
}

// 发送 AT 命令并获取响应
String sendATCommand(const char* cmd, unsigned long timeout) {
  while (Serial1.available()) Serial1.read();
  Serial1.println(cmd);
  
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < timeout) {
    while (Serial1.available()) {
      char c = Serial1.read();
      resp += c;
      if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) {
        delay(50);  // 等待剩余数据
        while (Serial1.available()) resp += (char)Serial1.read();
        return resp;
      }
    }
  }
  return resp;
}

// AT 命令并等待 OK
bool sendATandWaitOK(const char* cmd, unsigned long timeout) {
  while (Serial1.available()) Serial1.read();
  Serial1.println(cmd);
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < timeout) {
    while (Serial1.available()) {
      char c = Serial1.read();
      resp += c;
      if (resp.indexOf("OK") >= 0) return true;
      if (resp.indexOf("ERROR") >= 0) return false;
    }
  }
  return false;
}

// 等待 CGATT 附着
bool waitCGATT1() {
  Serial1.println("AT+CGATT?");
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < 2000) {
    while (Serial1.available()) {
      char c = Serial1.read();
      resp += c;
      if (resp.indexOf("+CGATT: 1") >= 0) return true;
      if (resp.indexOf("+CGATT: 0") >= 0) return false;
    }
  }
  return false;
}

// 重启模组
void resetModule() {
  Serial.println("正在重启模组...");
  Serial1.println("AT+CFUN=1,1");
  delay(3000);
}

// LED 闪烁
void blink_short(unsigned long gap_time) {
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(gap_time);
}

// 处理配置页面请求 (使用 chunked transfer 分块发送)
void handleRoot() {
  if (!checkAuth()) return;

  setNoCacheHeaders();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  
  // 发送 HTML 头部
  server.sendContent_P(htmlPage);
  yield();
  
  // 发送概览页 (带替换)
  String overview = FPSTR(HTML_PAGE_OVERVIEW);
  overview.replace("%IP%", WiFi.localIP().toString());
  String mqttStatusText = config.mqttEnabled ? (mqttClient.connected() ? "已连接" : "未连接") : "未启用";
  overview.replace("%MQTT_STATUS%", mqttStatusText);
  String mqttTopicsHtml = "";
  if (config.mqttEnabled && config.mqttServer.length() > 0) {
    mqttTopicsHtml = mqttTopicStatus + "<br>" + mqttTopicSmsReceived;
  } else {
    mqttTopicsHtml = "未配置";
  }
  overview.replace("%MQTT_TOPICS%", mqttTopicsHtml);
  server.sendContent(overview);
  yield();
  
  // 发送控制页 (带替换)
  String control = FPSTR(HTML_PAGE_CONTROL);
  control.replace("%AP_SW%", config.airplaneMode ? "on" : "");
  control.replace("%AP_BADGE%", config.airplaneMode ? "b-warn" : "b-ok");
  control.replace("%AP_STATUS%", config.airplaneMode ? "已开启" : "已关闭");
  control.replace("%SA_SW%", config.schedAirplaneEnabled ? "on" : "");
  control.replace("%SA_EN%", config.schedAirplaneEnabled ? "true" : "false");
  control.replace("%SA_DISP%", config.schedAirplaneEnabled ? "" : "display:none");
  control.replace("%SA_SH%", String(config.schedAirplaneStartHour));
  control.replace("%SA_SM%", String(config.schedAirplaneStartMin));
  control.replace("%SA_EH%", String(config.schedAirplaneEndHour));
  control.replace("%SA_EM%", String(config.schedAirplaneEndMin));
  server.sendContent(control);
  yield();
  
  // 发送历史页
  server.sendContent_P(HTML_PAGE_HISTORY);
  yield();
  
  // 发送配置页 Part1 (WiFi + SMTP)
  String cfg1 = FPSTR(HTML_PAGE_CONFIG1);
  String currentSsid = WiFi.SSID();
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    String idx = String(i);
    bool isCurrent = config.wifiNetworks[i].ssid.length() > 0 && config.wifiNetworks[i].ssid == currentSsid;
    cfg1.replace("%WF" + idx + "_BORDER%", isCurrent ? "var(--success)" : "#e2e8f0");
    cfg1.replace("%WF" + idx + "_CUR%", isCurrent ? "<span class=\"badge b-ok\">当前</span>" : "");
    cfg1.replace("%WF" + idx + "_SW%", config.wifiNetworks[i].enabled ? "on" : "");
    cfg1.replace("%WF" + idx + "_EN%", config.wifiNetworks[i].enabled ? "true" : "false");
    cfg1.replace("%WF" + idx + "_SSID%", config.wifiNetworks[i].ssid);
    cfg1.replace("%WF" + idx + "_HINT%", config.wifiNetworks[i].password.length() > 0 ? "（已保存，留空则保留）" : "WiFi密码");
  }
  cfg1.replace("%WEB_USER%", config.webUser);
  cfg1.replace("%WEB_PASS%", config.webPass);
  cfg1.replace("%SMTP_EN_VAL%", config.emailEnabled ? "true" : "false");
  cfg1.replace("%SMTP_EN_SW%", config.emailEnabled ? "on" : "");
  cfg1.replace("%SMTP_DISP%", config.emailEnabled ? "block" : "none");
  cfg1.replace("%SMTP_SERVER%", config.smtpServer);
  cfg1.replace("%SMTP_PORT%", String(config.smtpPort));
  cfg1.replace("%SMTP_USER%", config.smtpUser);
  cfg1.replace("%SMTP_PASS%", config.smtpPass);
  cfg1.replace("%SMTP_SEND_TO%", config.smtpSendTo);
  server.sendContent(cfg1);
  yield();
  
  // 发送配置页 Part2 (MQTT)
  String cfg2 = FPSTR(HTML_PAGE_CONFIG2);
  cfg2.replace("%MQTT_EN_VAL%", config.mqttEnabled ? "true" : "false");
  cfg2.replace("%MQTT_EN_SW%", config.mqttEnabled ? "on" : "");
  cfg2.replace("%MQTT_DISP%", config.mqttEnabled ? "block" : "none");
  cfg2.replace("%MQTT_SERVER%", config.mqttServer);
  cfg2.replace("%MQTT_PORT%", String(config.mqttPort));
  cfg2.replace("%MQTT_USER%", config.mqttUser);
  cfg2.replace("%MQTT_PASS%", config.mqttPass);
  cfg2.replace("%MQTT_PREFIX%", config.mqttPrefix);
  cfg2.replace("%MQTT_CO_VAL%", config.mqttControlOnly ? "true" : "false");
  cfg2.replace("%MQTT_CO_SW%", config.mqttControlOnly ? "on" : "");
  cfg2.replace("%MQTT_HA_VAL%", config.mqttHaDiscovery ? "true" : "false");
  cfg2.replace("%MQTT_HA_SW%", config.mqttHaDiscovery ? "on" : "");
  cfg2.replace("%MQTT_HA_DISP%", config.mqttHaDiscovery ? "block" : "none");
  cfg2.replace("%MQTT_HA_PREFIX%", config.mqttHaPrefix.length() > 0 ? config.mqttHaPrefix : "homeassistant");
  server.sendContent(cfg2);
  yield();
  
  // 发送配置页 Part3 (推送通道)
  String cfg3 = FPSTR(HTML_PAGE_CONFIG3);
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String idx = String(i);
    int tp = (int)config.pushChannels[i].type;
    cfg3.replace("%CH" + idx + "_SW%", config.pushChannels[i].enabled ? "on" : "");
    cfg3.replace("%CH" + idx + "_EN%", config.pushChannels[i].enabled ? "true" : "false");
    cfg3.replace("%CH" + idx + "_NAME%", config.pushChannels[i].name);
    cfg3.replace("%CH" + idx + "_URL%", config.pushChannels[i].url);
    cfg3.replace("%CH" + idx + "_K1%", config.pushChannels[i].key1);
    cfg3.replace("%CH" + idx + "_BODY%", config.pushChannels[i].customBody);
    for (int t = 1; t <= 7; t++) {
      cfg3.replace("%CH" + idx + "_T" + String(t) + "%", tp == t ? "selected" : "");
    }
    bool showK1 = (tp == 5 || tp == 7);
    cfg3.replace("%CH" + idx + "_K1D%", showK1 ? "block" : "none");
    cfg3.replace("%CH" + idx + "_K1L%", tp == 5 ? "Chat ID" : "加签密钥 (可选)");
    cfg3.replace("%CH" + idx + "_CFD%", tp == 4 ? "block" : "none");
  }
  server.sendContent(cfg3);
  yield();
  
  // 发送配置页 Part4 (过滤 + 定时任务)
  String cfg4 = FPSTR(HTML_PAGE_CONFIG4);
  cfg4.replace("%FILTER_EN_VAL%", config.filterEnabled ? "true" : "false");
  cfg4.replace("%FILTER_WL_VAL%", config.filterIsWhitelist ? "true" : "false");
  cfg4.replace("%CF_EN_VAL%", config.contentFilterEnabled ? "true" : "false");
  cfg4.replace("%CF_WL_VAL%", config.contentFilterIsWhitelist ? "true" : "false");
  cfg4.replace("%TIMER_EN_VAL%", config.timerEnabled ? "true" : "false");
  server.sendContent(cfg4);
  yield();
  
  // 发送 JavaScript Part1
  String js1 = FPSTR(HTML_JS_PART1);
  js1.replace("%FILTER_EN_BOOL%", config.filterEnabled ? "true" : "false");
  js1.replace("%FILTER_WL_BOOL%", config.filterIsWhitelist ? "true" : "false");
  String safeFilterList = config.filterList;
  safeFilterList.replace("\n", "");
  safeFilterList.replace("\r", "");
  js1.replace("%FILTER_LIST%", safeFilterList);
  js1.replace("%CF_EN_BOOL%", config.contentFilterEnabled ? "true" : "false");
  js1.replace("%CF_WL_BOOL%", config.contentFilterIsWhitelist ? "true" : "false");
  String safeCfList = config.contentFilterList;
  safeCfList.replace("\n", "");
  safeCfList.replace("\r", "");
  js1.replace("%CF_LIST%", safeCfList);
  js1.replace("%TIMER_EN_BOOL%", config.timerEnabled ? "true" : "false");
  js1.replace("%TIMER_TP%", String(config.timerType));
  js1.replace("%TIMER_INT%", String(config.timerInterval));
  js1.replace("%TIMER_PH%", config.timerPhone);
  js1.replace("%TIMER_MS%", config.timerMessage);
  js1.replace("%TIMER_START%", config.timerStartDate);
  server.sendContent(js1);
  yield();
  
  // 发送 JavaScript Part2 和 Part3
  server.sendContent_P(HTML_JS_PART2);
  yield();
  server.sendContent_P(HTML_JS_PART3);
  
  server.sendContent("");  // 结束 chunked 传输
}

// 处理工具箱页面请求 (重定向到首页)
void handleToolsPage() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// 处理保存配置请求 (全量保存)
void handleSave() {
  if (!checkAuth()) return;
  
  yield();
  
  // WiFi 网络配置
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    String prefix = "wifi" + String(i);
    String newSsid = server.arg(prefix + "ssid");
    String newPass = server.arg(prefix + "pass");
    String newEn = server.arg(prefix + "en");
    
    // 只有当 SSID 非空时才更新（避免清空已保存的配置）
    if (newSsid.length() > 0) {
      config.wifiNetworks[i].ssid = newSsid;
    }
    // 密码为空时保留原密码
    if (newPass.length() > 0) {
      config.wifiNetworks[i].password = newPass;
    }
    // enabled 状态始终更新
    config.wifiNetworks[i].enabled = (newEn == "true");
    
    // 调试输出
    Serial.printf("WiFi%d: SSID=%s, Pass=%s, En=%d\n", 
      i, config.wifiNetworks[i].ssid.c_str(), 
      config.wifiNetworks[i].password.length() > 0 ? "***" : "(empty)",
      config.wifiNetworks[i].enabled);
  }
  
  
  yield();
  
  // 获取新的 Web 账号密码
  String newWebUser = server.arg("webUser");
  String newWebPass = server.arg("webPass");
  if (newWebUser.length() > 0 && newWebPass.length() > 0) {
    config.webUser = newWebUser;
    config.webPass = newWebPass;
  }
  
  // SMTP 配置
  config.emailEnabled = server.arg("smtpEn") == "true";
  config.smtpServer = server.arg("smtpServer");
  config.smtpPort = server.arg("smtpPort").toInt();
  if (config.smtpPort == 0) config.smtpPort = 465;
  config.smtpUser = server.arg("smtpUser");
  config.smtpPass = server.arg("smtpPass");
  config.smtpSendTo = server.arg("smtpSendTo");
  
  
  yield();
  
  // 推送通道配置
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String prefix = "push" + String(i);
    config.pushChannels[i].enabled = server.arg(prefix + "en") == "true";
    config.pushChannels[i].type = (PushType)server.arg(prefix + "type").toInt();
    config.pushChannels[i].url = server.arg(prefix + "url");
    config.pushChannels[i].name = server.arg(prefix + "name");
    config.pushChannels[i].key1 = server.arg(prefix + "k1"); // Telegram Chat ID
    config.pushChannels[i].customBody = server.arg(prefix + "body");
  }
  
  
  yield();
  
  // MQTT 配置
  config.mqttEnabled = server.arg("mqttEn") == "true";
  config.mqttServer = server.arg("mqttServer");
  config.mqttPort = server.arg("mqttPort").toInt();
  if (config.mqttPort == 0) config.mqttPort = 1883;
  config.mqttUser = server.arg("mqttUser");
  config.mqttPass = server.arg("mqttPass");
  config.mqttPrefix = server.arg("mqttPrefix");
  if (config.mqttPrefix.length() == 0) config.mqttPrefix = "sms";
  config.mqttControlOnly = server.arg("mqttCtrlOnly") == "on" || server.arg("mqttCtrlOnly") == "true"; 
  
  // HA 自动发现配置
  config.mqttHaDiscovery = server.arg("mqttHaDiscovery") == "true";
  config.mqttHaPrefix = server.arg("mqttHaPrefix");
  if (config.mqttHaPrefix.length() == 0) config.mqttHaPrefix = "homeassistant";
  
  
  yield();
  
  // 黑白名单配置 (全量保存时也更新)
  if (server.hasArg("filterEn")) { // 只有当前端提交了这些字段才更新
      config.filterEnabled = server.arg("filterEn") == "true";
      config.filterIsWhitelist = server.arg("filterIsWhitelist") == "true";
      config.filterList = server.arg("filterList");
      config.filterList.replace("\r", "");
      config.filterList.replace("\n", ","); // 换行转逗号
  }

  // 定时任务配置 (全量保存时)
  if (server.hasArg("timerEn")) {
      config.timerEnabled = server.arg("timerEn") == "true";
      config.timerType = server.arg("timerType").toInt();
      config.timerInterval = server.arg("timerInterval").toInt();
      if (config.timerInterval < 1) config.timerInterval = 1;
      config.timerPhone = server.arg("timerPhone");
      config.timerMessage = server.arg("timerMessage");
      config.timerStartDate = server.arg("timerStartDate");
      timerIntervalSec = (unsigned long)config.timerInterval * 24UL * 60UL * 60UL;
  }
  
  yield();
  saveConfig();
  
  yield();
  configValid = isConfigValid();
  
  // 返回成功响应，让前端询问用户是否重启
  String json = "{\"success\":true,\"message\":\"配置已保存\",\"needRestart\":true}";
  server.send(200, "application/json", json);
}

// 处理发送短信请求
void handleSendSms() {
  if (!checkAuth()) return;
  
  // 解析 JSON body (前端 postJ 发送 JSON 格式)
  String body = server.arg("plain");
  String phone = "";
  String content = "";
  
  // 简易 JSON 解析
  int phoneIdx = body.indexOf("\"phone\"");
  if (phoneIdx >= 0) {
    int colonIdx = body.indexOf(":", phoneIdx);
    int startQuote = body.indexOf("\"", colonIdx);
    int endQuote = body.indexOf("\"", startQuote + 1);
    if (startQuote >= 0 && endQuote > startQuote) {
      phone = body.substring(startQuote + 1, endQuote);
    }
  }
  
  int contentIdx = body.indexOf("\"content\"");
  if (contentIdx >= 0) {
    int colonIdx = body.indexOf(":", contentIdx);
    int startQuote = body.indexOf("\"", colonIdx);
    int endQuote = body.indexOf("\"", startQuote + 1);
    if (startQuote >= 0 && endQuote > startQuote) {
      content = body.substring(startQuote + 1, endQuote);
    }
  }
  
  phone.trim();
  content.trim();
  
  bool success = false;
  String resultMsg = "";
  
  if (phone.length() == 0) {
    resultMsg = "错误：请输入目标号码";
  } else if (content.length() == 0) {
    resultMsg = "错误：请输入短信内容";
  } else {
    Serial.println("网页端发送短信请求");
    Serial.println("目标号码: " + phone);
    Serial.println("短信内容: " + content);
    
    success = sendSMS(phone.c_str(), content.c_str());
    if (success) {
      stats.smsSent++;
      saveStats();
    }
    resultMsg = success ? "短信发送成功！" : "短信发送失败，请检查模组状态";
  }
  
  String json = "{";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + resultMsg + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理定时任务配置保存 (API: /timer)
void handleTimer() {
  if (!checkAuth()) return;
  
  String body = server.arg("plain");
  
  config.timerEnabled = body.indexOf("\"enabled\":true") >= 0;
  
  // 简易解析 JSON (假设标准格式)
  // type
  int typeIdx = body.indexOf("\"type\":");
  if (typeIdx > 0) config.timerType = body.substring(typeIdx+7).toInt();
  
  // interval
  int intIdx = body.indexOf("\"interval\":");
  if (intIdx > 0) config.timerInterval = body.substring(intIdx+11).toInt();
  if (config.timerInterval < 1) config.timerInterval = 1;
  
  // phone
  int phIdx = body.indexOf("\"phone\":\"");
  if (phIdx > 0) {
      int end = body.indexOf("\"", phIdx+9);
      config.timerPhone = body.substring(phIdx+9, end);
  }
  
  // message
  int msgIdx = body.indexOf("\"message\":\"");
  if (msgIdx > 0) {
      int end = body.indexOf("\"", msgIdx+11);
      config.timerMessage = body.substring(msgIdx+11, end);
  }
  
  // startDate (起始日期 YYYY-MM-DD)
  int startIdx = body.indexOf("\"startDate\":\"");
  if (startIdx > 0) {
      int end = body.indexOf("\"", startIdx+13);
      config.timerStartDate = body.substring(startIdx+13, end);
  }
  
  timerIntervalSec = (unsigned long)config.timerInterval * 24UL * 60UL * 60UL;
  saveConfig();
  
  String json = "{\"success\":true}";
  server.send(200, "application/json", json);
}

// 处理重启请求
void handleRestart() {
  if (!checkAuth()) return;
  server.send(200, "application/json", "{\"success\":true,\"message\":\"设备将在2秒后重启\"}");
  delay(2000);
  ESP.restart();
}

// 获取短信历史记录
void handleSmsHistory() {
  if (!checkAuth()) return;
  
  String historyJson = getSmsHistory();
  String json = "{\"history\":" + historyJson + "}";
  
  server.send(200, "application/json", json);
}

// 获取统计信息
void handleStats() {
  if (!checkAuth()) return;
  
  String json = "{";
  json += "\"received\":" + String(stats.smsReceived) + ",";
  json += "\"sent\":" + String(stats.smsSent) + ",";
  json += "\"pushOk\":" + String(stats.pushSuccess) + ",";
  json += "\"pushFail\":" + String(stats.pushFailed) + ",";
  json += "\"boots\":" + String(stats.bootCount) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"wifiRssi\":" + String(WiFi.RSSI());
  json += "}";
  
  server.send(200, "application/json", json);
}

// 保存黑白名单配置
void handleFilterSave() {
  if (!checkAuth()) return;
  
  String body = server.arg("plain");
  // 简单 JSON 解析
  config.filterEnabled = body.indexOf("\"enabled\":true") >= 0;
  config.filterIsWhitelist = body.indexOf("\"whitelist\":true") >= 0;
  
  // 解析 numbers 数组或字符串
  int numStart = body.indexOf("\"numbers\":");
  if (numStart > 0) {
      int arrStart = body.indexOf("[", numStart);
      int arrEnd = body.indexOf("]", numStart);
      if (arrStart > 0 && arrEnd > arrStart) {
          String arrStr = body.substring(arrStart + 1, arrEnd);
          // 将 JSON 数组 "a","b" 转换为 a,b 存入 filterList
          config.filterList = "";
          int lastQ = -1;
          while (true) {
              int startQ = arrStr.indexOf("\"", lastQ + 1);
              if (startQ < 0) break;
              int endQ = arrStr.indexOf("\"", startQ + 1);
              if (endQ < 0) break;
              if (config.filterList.length() > 0) config.filterList += ",";
              config.filterList += arrStr.substring(startQ + 1, endQ);
              lastQ = endQ;
          }
      }
  }
  
  saveConfig();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"号码名单已更新\"}");
}

// 保存内容关键词过滤配置
void handleContentFilterSave() {
  if (!checkAuth()) return;
  
  String body = server.arg("plain");
  // 简单 JSON 解析
  config.contentFilterEnabled = body.indexOf("\"enabled\":true") >= 0;
  config.contentFilterIsWhitelist = body.indexOf("\"whitelist\":true") >= 0;
  
  // 解析 keywords 字符串
  int kwStart = body.indexOf("\"keywords\":\"");
  if (kwStart > 0) {
    int valStart = kwStart + 12;  // 跳过 "keywords":"
    int valEnd = body.indexOf("\"", valStart);
    if (valEnd > valStart) {
      config.contentFilterList = body.substring(valStart, valEnd);
    }
  }
  
  saveConfig();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"关键词过滤已更新\"}");
}

// 处理飞行模式切换
void handleAirplane() {
  if (!checkAuth()) return;
  
  String body = server.arg("plain");
  bool enabled = body.indexOf("\"enabled\":true") >= 0;
  
  Serial.printf("飞行模式请求: %s\n", enabled ? "开启" : "关闭");
  
  setAirplaneMode(enabled);
  
  // 通过 MQTT 发布状态更新
  if (config.mqttEnabled && mqttClient.connected()) {
    publishAirplaneModeStatus();
  }
  
  String json = "{";
  json += "\"success\":true,";
  json += "\"enabled\":" + String(config.airplaneMode ? "true" : "false") + ",";
  json += "\"message\":\"" + String(config.airplaneMode ? "飞行模式已开启，蜂窝网络已断开" : "飞行模式已关闭，正在恢复网络") + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理定时飞行模式设置
void handleSchedAirplane() {
  if (!checkAuth()) return;
  
  String body = server.arg("plain");
  
  // 解析 JSON
  config.schedAirplaneEnabled = body.indexOf("\"enabled\":true") >= 0;
  
  // startH
  int idx = body.indexOf("\"startH\":");
  if (idx > 0) config.schedAirplaneStartHour = body.substring(idx + 9).toInt();
  
  // startM
  idx = body.indexOf("\"startM\":");
  if (idx > 0) config.schedAirplaneStartMin = body.substring(idx + 9).toInt();
  
  // endH
  idx = body.indexOf("\"endH\":");
  if (idx > 0) config.schedAirplaneEndHour = body.substring(idx + 7).toInt();
  
  // endM
  idx = body.indexOf("\"endM\":");
  if (idx > 0) config.schedAirplaneEndMin = body.substring(idx + 7).toInt();
  
  // 边界检查
  if (config.schedAirplaneStartHour < 0 || config.schedAirplaneStartHour > 23) config.schedAirplaneStartHour = 22;
  if (config.schedAirplaneStartMin < 0 || config.schedAirplaneStartMin > 59) config.schedAirplaneStartMin = 0;
  if (config.schedAirplaneEndHour < 0 || config.schedAirplaneEndHour > 23) config.schedAirplaneEndHour = 8;
  if (config.schedAirplaneEndMin < 0 || config.schedAirplaneEndMin > 59) config.schedAirplaneEndMin = 0;
  
  saveConfig();
  
  Serial.printf("定时飞行已更新: %s, %02d:%02d-%02d:%02d\n",
    config.schedAirplaneEnabled ? "启用" : "禁用",
    config.schedAirplaneStartHour, config.schedAirplaneStartMin,
    config.schedAirplaneEndHour, config.schedAirplaneEndMin);
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"定时飞行设置已保存\"}");
}
