#include "template_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_alarm_ {

static const char *const TAG = "template_alarm.text_sensor";

void TemplateTextSensor::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (val.has_value()) {
    this->publish_state(*val);
  }
}
float TemplateTextSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void TemplateTextSensor::set_template(std::function<optional<std::string>()> &&f) { this->f_ = f; }
void TemplateTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Template Alarm Sensor", this); }

}  // namespace template_alarm_
}  // namespace esphome
