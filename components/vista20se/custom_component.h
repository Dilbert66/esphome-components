#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"

#include <vector>

namespace esphome {
namespace custom_component {

class CustomComponentConstructor : public PollingComponent {
 public:
  CustomComponentConstructor(const std::function<Component *()> init) {
   c = init();
  }

  void setup() override{
    set_update_interval(8); //set looptime to 8ms 
    c->setup();
  }
  void update() override{
   c->loop();
  }

 protected:
  Component * c;
};

}  // namespace custom_component
}  // namespace esphome
