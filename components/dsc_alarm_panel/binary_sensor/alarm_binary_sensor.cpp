#include "alarm_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace alarm_panel
  {

    static const char *const TAG = "alarm.binary_sensor";

    void AlarmBinarySensor::setup()
    {

      if (!this->publish_initial_state_)
        return;

      if (this->f_ != nullptr)
      {
        this->publish_initial_state(this->f_().value_or(false));
      }
      else
      {
        this->publish_initial_state(false);
      }
    }

    void AlarmBinarySensor::loop()
    {
      if (this->f_ == nullptr)
        return;

      auto s = this->f_();
      if (s.has_value())
      {
        this->publish_state(*s);
      }
    }
    void AlarmBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Alarm Binary Sensor", this); }

  } // namespace alarm_panel
} // namespace esphome
