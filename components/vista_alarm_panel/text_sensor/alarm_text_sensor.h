#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace alarm_panel {

class AlarmTextSensor : public text_sensor::TextSensor, public Component {
   
};

}  // namespace alarm_panel
}  // namespace esphome
