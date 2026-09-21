#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

class BrightnessController {
 private:
  // Display refs are cheap identifiers, safe to cache. Handles are not: an
  // open handle holds libddcutil's per display mutex, which starves the watch
  // thread's own detection. Handles live only for one transaction.
  struct Display {
    DDCA_Display_Ref ref;
    std::string name;
    int consecutive_failures;
    int64_t benched_at_millis;
  };

  std::vector<Display> displays;
  // Written by libddcutil's watch thread, consumed by the keyboard thread.
  static std::atomic<bool> displays_stale;
  static std::atomic<int64_t> last_event_millis;
  // TODO Make them configurable
  const int BRIGHTNESS_STEP = 10;       // Adjust brightness by 10% each time
  const uint8_t VCP_BRIGHTNESS = 0x10;  // VCP code for brightness
  // A DP link can take seconds to retrain after an input switch. DDC rides the
  // same AUX channel, so touching the bus inside this window is what wedges a
  // monitor mid-negotiation.
  const int64_t SETTLE_MILLIS = 5000;
  const int MAX_CONSECUTIVE_FAILURES = 2;
  // A monitor that keeps HPD asserted while switched to another input never
  // raises a watch event, so a benched display gets an occasional retry.
  const int64_t BENCH_RETRY_MILLIS = 30000;

  static int64_t nowMillis();
  static void onDisplayStatusEvent(DDCA_Display_Status_Event event);
  void startWatchingDisplays();
  bool loadDisplays();
  bool redetectDisplays();
  bool isUsable(const Display& display);
  void noteResult(Display& display, DDCA_Status rc);
  DDCA_Status readBrightness(const Display& display, int* value);
  DDCA_Status writeBrightness(const Display& display, int value);
  void adjustBrightness(int delta);

 public:
  ~BrightnessController();
  bool init();
  void onBrightnessUp(int presses);
  void onBrightnessDown(int presses);
};
