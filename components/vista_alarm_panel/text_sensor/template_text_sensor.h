#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace alarm_sensor {

class TemplateTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_template(std::function<optional<std::string>()> &&f);

  void update() override;

  float get_setup_priority() const override;

  void dump_config() override;
  
 
 private:
 protected:
  optional<std::function<optional<std::string>()>> f_{};
   
};

}  // namespace alarm_sensor
}  // namespace esphome
