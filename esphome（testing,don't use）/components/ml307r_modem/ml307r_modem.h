#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <string>
#include <vector>

namespace esphome {
namespace ml307r_modem {

// 长短信分段结构
struct SmsPart {
  bool valid = false;
  std::string text;
};

// 长短信缓存结构
struct ConcatSms {
  bool in_use = false;
  int ref_number = 0;
  std::string sender;
  std::string timestamp;
  int total_parts = 0;
  int received_parts = 0;
  uint32_t first_part_time = 0;
  SmsPart parts[10];  // 最多10个分段
};

class ML307RModem : public Component, public uart::UARTDevice {
 public:
  // 初始化
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // 设置传感器
  void set_sms_sender_sensor(text_sensor::TextSensor *sensor) { sms_sender_sensor_ = sensor; }
  void set_sms_content_sensor(text_sensor::TextSensor *sensor) { sms_content_sensor_ = sensor; }
  void set_sms_timestamp_sensor(text_sensor::TextSensor *sensor) { sms_timestamp_sensor_ = sensor; }
  void set_signal_strength_sensor(sensor::Sensor *sensor) { signal_strength_sensor_ = sensor; }
  void set_network_status_sensor(text_sensor::TextSensor *sensor) { network_status_sensor_ = sensor; }
  void set_modem_online_sensor(binary_sensor::BinarySensor *sensor) { modem_online_sensor_ = sensor; }

  // AT 命令相关
  bool send_at_command(const std::string &cmd, uint32_t timeout_ms = 1000);
  bool send_at_and_wait_ok(const std::string &cmd, uint32_t timeout_ms = 1000);
  
  // 短信功能
  bool send_sms(const std::string &phone_number, const std::string &message);
  
  // 查询功能
  void query_signal_strength();
  void query_network_status();
  void do_ping(const std::string &host = "8.8.8.8");

 protected:
  // 传感器指针
  text_sensor::TextSensor *sms_sender_sensor_{nullptr};
  text_sensor::TextSensor *sms_content_sensor_{nullptr};
  text_sensor::TextSensor *sms_timestamp_sensor_{nullptr};
  sensor::Sensor *signal_strength_sensor_{nullptr};
  text_sensor::TextSensor *network_status_sensor_{nullptr};
  binary_sensor::BinarySensor *modem_online_sensor_{nullptr};

  // 状态变量
  bool modem_initialized_{false};
  bool waiting_for_pdu_{false};
  std::string line_buffer_;
  uint32_t last_status_query_{0};
  
  // 长短信缓存
  static const int MAX_CONCAT_MESSAGES = 5;
  ConcatSms concat_buffer_[MAX_CONCAT_MESSAGES];
  
  // 统计
  uint32_t sms_received_count_{0};
  uint32_t sms_sent_count_{0};

  // 内部方法
  void process_line_(const std::string &line);
  void process_pdu_(const std::string &pdu_data);
  void process_sms_content_(const std::string &sender, const std::string &message, const std::string &timestamp);
  
  // PDU 解码相关
  std::string decode_pdu_(const std::string &pdu);
  std::string decode_gsm7_(const uint8_t *data, size_t len);
  std::string decode_ucs2_(const uint8_t *data, size_t len);
  int hex_to_int_(char c);
  uint8_t hex_pair_to_byte_(char h, char l);
  
  // 长短信处理
  int find_or_create_concat_slot_(int ref_number, const std::string &sender, int total_parts);
  std::string assemble_concat_sms_(int slot);
  void clear_concat_slot_(int slot);
  void check_concat_timeout_();
  
  // 时间戳格式化
  std::string format_timestamp_(const std::string &pdu_timestamp);
  
  // 辅助函数
  bool is_hex_string_(const std::string &str);
  std::string read_line_();
};

}  // namespace ml307r_modem
}  // namespace esphome
