#include "BrightnessController.h"

#include <chrono>

std::atomic<bool> BrightnessController::displays_stale{false};
std::atomic<int64_t> BrightnessController::last_event_millis{0};

int64_t BrightnessController::nowMillis() {
  auto since_epoch = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch)
      .count();
}

void BrightnessController::onDisplayStatusEvent(
    DDCA_Display_Status_Event event) {
  std::cout << "Display event: "
            << ddca_display_event_type_name(event.event_type) << " on "
            << event.connector_name << std::endl;

  // Runs on libddcutil's watch thread, so only record the change here.
  // Rediscovery happens on the keyboard thread, which keeps display access
  // single threaded and collapses the burst of events an MST chain emits into
  // one rescan. ddca_redetect_displays() also refuses recursive calls.
  if (event.event_type == DDCA_EVENT_DISPLAY_CONNECTED ||
      event.event_type == DDCA_EVENT_DISPLAY_DISCONNECTED ||
      event.event_type == DDCA_EVENT_DDC_ENABLED) {
    last_event_millis.store(nowMillis());
    displays_stale.store(true);
  }
}

BrightnessController::~BrightnessController() {
  ddca_stop_watch_displays(false);
  ddca_unregister_display_status_callback(onDisplayStatusEvent);
}

bool BrightnessController::init() {
  // Initialize libddcutil
  DDCA_Status rc =
      ddca_init(nullptr, DDCA_SYSLOG_NOT_SET, DDCA_INIT_OPTIONS_NONE);
  if (rc != 0) {
    std::cerr << "ddca_init() failed: " << ddca_rc_name(rc) << std::endl;
    return false;
  }

  if (!loadDisplays()) {
    return false;
  }

  startWatchingDisplays();
  return true;
}

void BrightnessController::startWatchingDisplays() {
  DDCA_Status rc = ddca_register_display_status_callback(onDisplayStatusEvent);
  if (rc != 0) {
    std::cerr << "Hotplug detection unavailable: " << ddca_rc_name(rc)
              << " (displays will only be rescanned after a DDC error)"
              << std::endl;
    return;
  }

  // libddcutil defaults to no stabilization delay, so it reports a connect the
  // instant udev fires, while the DP link is still retraining. Have it wait for
  // /sys/class/drm to settle before telling us anything.
  DDCA_DW_Settings watch_settings;
  if (ddca_get_display_watch_settings(&watch_settings) == 0) {
    watch_settings.initial_stabilization_millisec = 1000;
    DDCA_Status settings_rc = ddca_set_display_watch_settings(&watch_settings);
    if (settings_rc != 0) {
      std::cerr << "Could not set display watch settings: "
                << ddca_rc_name(settings_rc) << std::endl;
    }
  }

  rc = ddca_start_watch_displays(DDCA_EVENT_CLASS_DISPLAY_CONNECTION);
  if (rc != 0) {
    std::cerr << "ddca_start_watch_displays() failed: " << ddca_rc_name(rc)
              << std::endl;
    ddca_unregister_display_status_callback(onDisplayStatusEvent);
    return;
  }

  std::cout << "Watching for display connection changes" << std::endl;
}

bool BrightnessController::loadDisplays() {
  displays.clear();

  DDCA_Display_Info_List* dlist = nullptr;
  DDCA_Status rc = ddca_get_display_info_list2(false, &dlist);
  if (rc != 0) {
    std::cerr << "ddca_get_display_info_list2() failed: " << ddca_rc_name(rc)
              << std::endl;
    return false;
  }

  for (int i = 0; i < dlist->ct; i++) {
    displays.push_back(Display{dlist->info[i].dref, dlist->info[i].model_name,
                               0, 0});
    std::cout << "Display " << (i + 1) << ": " << dlist->info[i].model_name
              << std::endl;
  }

  // The refs outlive the list; only the list itself is owned by the caller.
  ddca_free_display_info_list(dlist);

  if (displays.empty()) {
    std::cerr << "No DDC capable displays found" << std::endl;
    return false;
  }

  std::cout << "Tracking " << displays.size() << " display(s)" << std::endl;
  return true;
}

bool BrightnessController::redetectDisplays() {
  // Invalidates every existing display ref and rescans the i2c buses. Safe
  // here only because no handle is held open outside a transaction.
  DDCA_Status rc = ddca_redetect_displays();
  if (rc != 0) {
    std::cerr << "ddca_redetect_displays() failed: " << ddca_rc_name(rc)
              << std::endl;
    displays.clear();
    return false;
  }

  return loadDisplays();
}

bool BrightnessController::isUsable(const Display& display) {
  if (display.consecutive_failures < MAX_CONSECUTIVE_FAILURES) return true;
  return nowMillis() - display.benched_at_millis >= BENCH_RETRY_MILLIS;
}

void BrightnessController::noteResult(Display& display, DDCA_Status rc) {
  if (rc == 0) {
    display.consecutive_failures = 0;
    return;
  }

  // Brief contention with the watch thread says nothing about the display.
  if (rc == DDCRC_LOCKED) return;

  display.consecutive_failures++;
  display.benched_at_millis = nowMillis();

  std::cerr << "DDC failure on " << display.name << ": " << ddca_rc_name(rc)
            << std::endl;

  if (display.consecutive_failures == MAX_CONSECUTIVE_FAILURES) {
    std::cerr << "  " << display.name << " is not responding, skipping it"
              << " until a display event or retry window" << std::endl;
  }
}

DDCA_Status BrightnessController::readBrightness(const Display& display,
                                                 int* value) {
  DDCA_Display_Handle dh = nullptr;
  // wait == false: never block the keyboard thread on the watch thread.
  DDCA_Status rc = ddca_open_display2(display.ref, false, &dh);
  if (rc != 0) return rc;

  DDCA_Non_Table_Vcp_Value valrec;
  rc = ddca_get_non_table_vcp_value(dh, VCP_BRIGHTNESS, &valrec);
  if (rc == 0) {
    *value = valrec.sh << 8 | valrec.sl;  // Current value
  }

  ddca_close_display(dh);
  return rc;
}

DDCA_Status BrightnessController::writeBrightness(const Display& display,
                                                  int value) {
  DDCA_Display_Handle dh = nullptr;
  DDCA_Status rc = ddca_open_display2(display.ref, false, &dh);
  if (rc != 0) return rc;

  rc = ddca_set_non_table_vcp_value(dh, VCP_BRIGHTNESS, 0, value);

  ddca_close_display(dh);
  return rc;
}

void BrightnessController::adjustBrightness(int delta) {
  // Stay off the bus entirely while a link is still coming up. Rescanning or
  // writing VCP values mid-retrain can leave the monitor without an image.
  int64_t since_event = nowMillis() - last_event_millis.load();
  if (displays_stale.load() && since_event < SETTLE_MILLIS) {
    std::cout << "Display link still settling, ignoring keypress" << std::endl;
    return;
  }

  if (displays_stale.exchange(false)) {
    std::cout << "Display configuration changed, rescanning" << std::endl;
    redetectDisplays();
  }

  int current = 0;
  bool read_ok = false;

  // Current brightness comes from the first display that answers.
  for (Display& display : displays) {
    if (!isUsable(display)) continue;

    DDCA_Status rc = readBrightness(display, &current);
    noteResult(display, rc);
    if (rc == 0) {
      read_ok = true;
      break;
    }
  }

  if (!read_ok) {
    std::cerr << "No display could report its brightness" << std::endl;
    return;
  }

  int target = current + delta;
  if (target < 0) target = 0;
  if (target > 100) target = 100;

  bool write_ok = false;
  for (Display& display : displays) {
    if (!isUsable(display)) continue;

    DDCA_Status rc = writeBrightness(display, target);
    noteResult(display, rc);
    if (rc == 0) write_ok = true;
  }

  if (write_ok) {
    std::cout << "   " << current << "% → " << target << "%" << std::endl;
  }
}

void BrightnessController::onBrightnessUp(int presses) {
  std::cout << "🔆 Brightness UP key pressed! (x" << presses << ")"
            << std::endl;
  adjustBrightness(BRIGHTNESS_STEP * presses);
}

void BrightnessController::onBrightnessDown(int presses) {
  std::cout << "🔅 Brightness DOWN key pressed! (x" << presses << ")"
            << std::endl;
  adjustBrightness(-BRIGHTNESS_STEP * presses);
}
