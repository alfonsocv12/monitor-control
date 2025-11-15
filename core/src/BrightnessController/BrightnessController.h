#include <ddcutil_c_api.h>

#include <iostream>
#include <vector>

class BrightnessController {
 private:
  std::vector<DDCA_Display_Handle> display_handles;
  // TODO Make them configurable
  const int BRIGHTNESS_STEP = 10;       // Adjust brightness by 10% each time
  const uint8_t VCP_BRIGHTNESS = 0x10;  // VCP code for brightness

  bool initDDCUtil();
  int getCurrentBrightness();
  bool setBrightness(int value);

 public:
  bool init();
  void onBrightnessUp();
  void onBrightnessDown();
};
