#include <functional>
#include <iostream>

#include "BrightnessController/BrightnessController.h"
#include "KeyboardController/KeyboardController.h"

using std::bind;

int main() {
  KeyboardController keyboard;
  BrightnessController monitorCtrl;

  if (!monitorCtrl.init()) {
    std::cerr << "Error while discovering monitors" << std::endl;
    return 0;
  }

  if (!keyboard.init()) {
    std::cerr << "Error while discovering keyboards" << std::endl;
    return 0;
  }

  KBindings bindings = {
      Binding{KEY_BRIGHTNESSUP,
              bind(&BrightnessController::onBrightnessUp, &monitorCtrl)},
      Binding{KEY_BRIGHTNESSDOWN,
              bind(&BrightnessController::onBrightnessDown, &monitorCtrl)}};

  keyboard.monitor(bindings);
}
