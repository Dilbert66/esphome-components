#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace template_alarm_ {

class TemplateTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_template(std::function<optional<std::string>()> &&f);

  void update() override;

  float get_setup_priority() const override;

  void dump_config() override;
  
  void set_type_id( const char *id) {this->type_id_str_=id;}
  std::string get_type_id(){  if (this->type_id_str_ == nullptr) {
    return ""; }  return this->type_id_str_;}

 protected:
  optional<std::function<optional<std::string>()>> f_{};
  const char * type_id_str_{nullptr};    
};

}  // namespace template_alarm_
}  // namespace esphome
