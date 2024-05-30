#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace template_alarm_ {

class TemplateBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void set_template(std::function<optional<bool>()> &&f) { this->f_ = f; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  
    void set_type_id( const char *id) {this->type_id_str_=id;}
  std::string get_type_id(){  if (this->type_id_str_ == nullptr) {
    return ""; }  return this->type_id_str_;}

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  std::function<optional<bool>()> f_{nullptr};
  const char * type_id_str_{nullptr};  
};

}  // namespace template_alarm_
}  // namespace esphome
