#include "template_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alarm_sensor {

static const char *const TAG = "alarm.text_sensor";

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
void TemplateTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Alarm Text Sensor", this); }

}  // namespace alarm_sensor
}  // namespace esphome
