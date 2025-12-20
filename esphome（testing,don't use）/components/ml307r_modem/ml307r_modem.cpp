#include "ml307r_modem.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ml307r_modem {

static const char *const TAG = "ml307r_modem";

// 长短信超时时间 (30秒)
static const uint32_t CONCAT_TIMEOUT_MS = 30000;

void ML307RModem::setup() {
  ESP_LOGI(TAG, "初始化 ML307R 4G 模块...");
  
  // 清空长短信缓存
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    concat_buffer_[i].in_use = false;
    concat_buffer_[i].received_parts = 0;
  }
  
  // 等待模块启动
  delay(2000);
  
  // 发送 AT 测试
  int retry = 0;
  while (!send_at_and_wait_ok("AT", 1000) && retry < 10) {
    ESP_LOGW(TAG, "AT 未响应，重试...");
    delay(500);
    retry++;
  }
  
  if (retry >= 10) {
    ESP_LOGE(TAG, "模块无响应，请检查连接");
    if (modem_online_sensor_ != nullptr) {
      modem_online_sensor_->publish_state(false);
    }
    return;
  }
  
  ESP_LOGI(TAG, "模块 AT 响应正常");
  
  // 重置 APN（自动识别）
  send_at_and_wait_ok("AT+CGDCONT=1,\"IP\",\"\"", 2000);
  
  // 设置短信自动上报模式
  if (!send_at_and_wait_ok("AT+CNMI=2,2,0,0,0", 2000)) {
    ESP_LOGW(TAG, "AT+CNMI 设置失败");
  }
  
  // 设置 PDU 模式
  if (!send_at_and_wait_ok("AT+CMGF=0", 2000)) {
    ESP_LOGW(TAG, "AT+CMGF 设置失败");
  }
  
  // 等待网络附着
  retry = 0;
  bool attached = false;
  while (!attached && retry < 30) {
    this->write_str("AT+CGATT?\r\n");
    delay(1000);
    std::string response = read_line_();
    if (response.find("+CGATT: 1") != std::string::npos) {
      attached = true;
    }
    retry++;
  }
  
  if (attached) {
    ESP_LOGI(TAG, "网络已附着");
  } else {
    ESP_LOGW(TAG, "网络附着超时");
  }
  
  modem_initialized_ = true;
  
  if (modem_online_sensor_ != nullptr) {
    modem_online_sensor_->publish_state(true);
  }
  
  ESP_LOGI(TAG, "ML307R 初始化完成");
}

void ML307RModem::loop() {
  // 读取 UART 数据
  while (this->available()) {
    char c = this->read();
    
    if (c == '\n') {
      // 处理完整的一行
      if (!line_buffer_.empty() && line_buffer_.back() == '\r') {
        line_buffer_.pop_back();
      }
      if (!line_buffer_.empty()) {
        process_line_(line_buffer_);
      }
      line_buffer_.clear();
    } else if (c != '\r') {
      line_buffer_ += c;
      // 防止缓冲区溢出
      if (line_buffer_.length() > 500) {
        line_buffer_.clear();
      }
    }
  }
  
  // 检查长短信超时
  check_concat_timeout_();
  
  // 定期查询状态 (每60秒)
  uint32_t now = millis();
  if (modem_initialized_ && now - last_status_query_ > 60000) {
    last_status_query_ = now;
    query_signal_strength();
  }
}

void ML307RModem::dump_config() {
  ESP_LOGCONFIG(TAG, "ML307R 4G Modem:");
  ESP_LOGCONFIG(TAG, "  已初始化: %s", modem_initialized_ ? "是" : "否");
}

void ML307RModem::process_line_(const std::string &line) {
  ESP_LOGD(TAG, "收到: %s", line.c_str());
  
  if (waiting_for_pdu_) {
    // 等待 PDU 数据
    if (is_hex_string_(line)) {
      process_pdu_(line);
    } else {
      ESP_LOGW(TAG, "预期 PDU 数据，但收到: %s", line.c_str());
    }
    waiting_for_pdu_ = false;
  } else if (line.rfind("+CMT:", 0) == 0) {
    // 收到短信通知，等待 PDU 数据
    ESP_LOGI(TAG, "检测到 +CMT，等待 PDU 数据...");
    waiting_for_pdu_ = true;
  } else if (line.rfind("+CSQ:", 0) == 0) {
    // 信号强度响应: +CSQ: rssi,ber
    size_t pos = line.find(':');
    if (pos != std::string::npos) {
      std::string value = line.substr(pos + 2);
      size_t comma = value.find(',');
      if (comma != std::string::npos) {
        int rssi = atoi(value.substr(0, comma).c_str());
        // 转换为 dBm: rssi * 2 - 113
        if (rssi < 99) {
          int dbm = rssi * 2 - 113;
          if (signal_strength_sensor_ != nullptr) {
            signal_strength_sensor_->publish_state(dbm);
          }
        }
      }
    }
  } else if (line.rfind("+CGATT:", 0) == 0) {
    // 网络附着状态
    bool attached = line.find("1") != std::string::npos;
    if (network_status_sensor_ != nullptr) {
      network_status_sensor_->publish_state(attached ? "已附着" : "未附着");
    }
  }
}

bool ML307RModem::send_at_command(const std::string &cmd, uint32_t timeout_ms) {
  // 清空接收缓冲
  while (this->available()) {
    this->read();
  }
  
  // 发送命令
  this->write_str((cmd + "\r\n").c_str());
  ESP_LOGD(TAG, "发送: %s", cmd.c_str());
  
  return true;
}

bool ML307RModem::send_at_and_wait_ok(const std::string &cmd, uint32_t timeout_ms) {
  send_at_command(cmd, timeout_ms);
  
  uint32_t start = millis();
  std::string response;
  
  while (millis() - start < timeout_ms) {
    while (this->available()) {
      char c = this->read();
      response += c;
      
      if (response.find("OK") != std::string::npos) {
        return true;
      }
      if (response.find("ERROR") != std::string::npos) {
        return false;
      }
    }
    delay(10);
  }
  
  return false;
}

bool ML307RModem::send_sms(const std::string &phone_number, const std::string &message) {
  ESP_LOGI(TAG, "发送短信到 %s", phone_number.c_str());
  
  // 切换到 Text 模式 (简化实现)
  if (!send_at_and_wait_ok("AT+CMGF=1", 1000)) {
    ESP_LOGE(TAG, "切换到 Text 模式失败");
    return false;
  }
  
  // 发送 CMGS 命令
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", phone_number.c_str());
  this->write_str(cmd);
  this->write_str("\r\n");
  
  // 等待 > 提示符
  uint32_t start = millis();
  bool got_prompt = false;
  while (millis() - start < 5000) {
    if (this->available()) {
      char c = this->read();
      if (c == '>') {
        got_prompt = true;
        break;
      }
    }
  }
  
  if (!got_prompt) {
    ESP_LOGE(TAG, "未收到 > 提示符");
    send_at_and_wait_ok("AT+CMGF=0", 1000);  // 切回 PDU 模式
    return false;
  }
  
  // 发送消息内容
  this->write_str(message.c_str());
  this->write_byte(0x1A);  // Ctrl+Z
  
  // 等待 OK
  bool success = false;
  start = millis();
  std::string response;
  while (millis() - start < 30000) {
    while (this->available()) {
      response += (char)this->read();
      if (response.find("OK") != std::string::npos) {
        success = true;
        break;
      }
      if (response.find("ERROR") != std::string::npos) {
        break;
      }
    }
    if (success || response.find("ERROR") != std::string::npos) {
      break;
    }
    delay(10);
  }
  
  // 切回 PDU 模式
  send_at_and_wait_ok("AT+CMGF=0", 1000);
  
  if (success) {
    sms_sent_count_++;
    ESP_LOGI(TAG, "短信发送成功");
  } else {
    ESP_LOGE(TAG, "短信发送失败");
  }
  
  return success;
}

void ML307RModem::query_signal_strength() {
  send_at_command("AT+CSQ");
}

void ML307RModem::query_network_status() {
  send_at_command("AT+CGATT?");
}

void ML307RModem::do_ping(const std::string &host) {
  ESP_LOGI(TAG, "执行 Ping 测试: %s", host.c_str());
  send_at_and_wait_ok("AT+CGACT=1,1", 10000);
  
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+MPING=1,\"%s\",4,32,255", host.c_str());
  send_at_command(cmd);
  
  delay(5000);
  send_at_and_wait_ok("AT+CGACT=0,1", 5000);
}

// ============ PDU 解码相关 ============

void ML307RModem::process_pdu_(const std::string &pdu_data) {
  ESP_LOGI(TAG, "解析 PDU 数据 (%d 字节)", pdu_data.length());
  
  // 简化的 PDU 解码
  // 完整实现需要参考 pdulib 库
  
  if (pdu_data.length() < 20) {
    ESP_LOGW(TAG, "PDU 数据太短");
    return;
  }
  
  // 这里是简化版本，实际需要完整的 PDU 解码
  // 建议使用原项目的 pdulib 库
  
  // 暂时发布原始 PDU 数据
  sms_received_count_++;
  
  if (sms_content_sensor_ != nullptr) {
    sms_content_sensor_->publish_state("[PDU] " + pdu_data.substr(0, 50) + "...");
  }
  
  ESP_LOGI(TAG, "收到短信 (需要 PDU 解码)");
}

void ML307RModem::process_sms_content_(const std::string &sender, 
                                        const std::string &message, 
                                        const std::string &timestamp) {
  ESP_LOGI(TAG, "处理短信: 发送者=%s, 时间=%s", sender.c_str(), timestamp.c_str());
  ESP_LOGI(TAG, "内容: %s", message.c_str());
  
  // 更新传感器
  if (sms_sender_sensor_ != nullptr) {
    sms_sender_sensor_->publish_state(sender);
  }
  if (sms_content_sensor_ != nullptr) {
    sms_content_sensor_->publish_state(message);
  }
  if (sms_timestamp_sensor_ != nullptr) {
    sms_timestamp_sensor_->publish_state(timestamp);
  }
}

int ML307RModem::hex_to_int_(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

uint8_t ML307RModem::hex_pair_to_byte_(char h, char l) {
  return (hex_to_int_(h) << 4) | hex_to_int_(l);
}

bool ML307RModem::is_hex_string_(const std::string &str) {
  if (str.empty()) return false;
  for (char c : str) {
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

std::string ML307RModem::read_line_() {
  std::string line;
  uint32_t start = millis();
  
  while (millis() - start < 1000) {
    while (this->available()) {
      char c = this->read();
      if (c == '\n') {
        return line;
      } else if (c != '\r') {
        line += c;
      }
    }
    delay(10);
  }
  
  return line;
}

// ============ 长短信处理 ============

int ML307RModem::find_or_create_concat_slot_(int ref_number, const std::string &sender, int total_parts) {
  // 查找已存在的槽位
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concat_buffer_[i].in_use && 
        concat_buffer_[i].ref_number == ref_number &&
        concat_buffer_[i].sender == sender) {
      return i;
    }
  }
  
  // 查找空闲槽位
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (!concat_buffer_[i].in_use) {
      concat_buffer_[i].in_use = true;
      concat_buffer_[i].ref_number = ref_number;
      concat_buffer_[i].sender = sender;
      concat_buffer_[i].total_parts = total_parts;
      concat_buffer_[i].received_parts = 0;
      concat_buffer_[i].first_part_time = millis();
      for (int j = 0; j < 10; j++) {
        concat_buffer_[i].parts[j].valid = false;
        concat_buffer_[i].parts[j].text.clear();
      }
      return i;
    }
  }
  
  // 覆盖最老的槽位
  int oldest = 0;
  uint32_t oldest_time = concat_buffer_[0].first_part_time;
  for (int i = 1; i < MAX_CONCAT_MESSAGES; i++) {
    if (concat_buffer_[i].first_part_time < oldest_time) {
      oldest_time = concat_buffer_[i].first_part_time;
      oldest = i;
    }
  }
  
  concat_buffer_[oldest].in_use = true;
  concat_buffer_[oldest].ref_number = ref_number;
  concat_buffer_[oldest].sender = sender;
  concat_buffer_[oldest].total_parts = total_parts;
  concat_buffer_[oldest].received_parts = 0;
  concat_buffer_[oldest].first_part_time = millis();
  
  return oldest;
}

std::string ML307RModem::assemble_concat_sms_(int slot) {
  std::string result;
  for (int i = 0; i < concat_buffer_[slot].total_parts; i++) {
    if (concat_buffer_[slot].parts[i].valid) {
      result += concat_buffer_[slot].parts[i].text;
    } else {
      result += "[缺失分段" + std::to_string(i + 1) + "]";
    }
  }
  return result;
}

void ML307RModem::clear_concat_slot_(int slot) {
  concat_buffer_[slot].in_use = false;
  concat_buffer_[slot].received_parts = 0;
  concat_buffer_[slot].sender.clear();
  concat_buffer_[slot].timestamp.clear();
}

void ML307RModem::check_concat_timeout_() {
  uint32_t now = millis();
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concat_buffer_[i].in_use) {
      if (now - concat_buffer_[i].first_part_time >= CONCAT_TIMEOUT_MS) {
        ESP_LOGW(TAG, "长短信超时，强制处理");
        
        std::string full_text = assemble_concat_sms_(i);
        process_sms_content_(concat_buffer_[i].sender, full_text, concat_buffer_[i].timestamp);
        clear_concat_slot_(i);
      }
    }
  }
}

std::string ML307RModem::format_timestamp_(const std::string &pdu_timestamp) {
  if (pdu_timestamp.length() < 12) return pdu_timestamp;
  
  int year = 2000 + atoi(pdu_timestamp.substr(0, 2).c_str());
  int month = atoi(pdu_timestamp.substr(2, 2).c_str());
  int day = atoi(pdu_timestamp.substr(4, 2).c_str());
  int hour = atoi(pdu_timestamp.substr(6, 2).c_str());
  int minute = atoi(pdu_timestamp.substr(8, 2).c_str());
  int second = atoi(pdu_timestamp.substr(10, 2).c_str());
  
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", 
           year, month, day, hour, minute, second);
  return std::string(buf);
}

}  // namespace ml307r_modem
}  // namespace esphome
