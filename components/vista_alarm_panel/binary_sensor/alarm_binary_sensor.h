#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace alarm_panel {


class AlarmBinarySensor : public Component, public binary_sensor::BinarySensor{
  
};

}  // namespace alarm_panel
}  // namespace esphome
