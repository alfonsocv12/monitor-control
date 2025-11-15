#include "BrightnessController.h"

bool BrightnessController::init() {
  // Initialize libddcutil
  DDCA_Status rc =
      ddca_init(nullptr, DDCA_SYSLOG_NOT_SET, DDCA_INIT_OPTIONS_NONE);
  if (rc != 0) {
    std::cerr << "ddca_init() failed: " << ddca_rc_name(rc) << std::endl;
    return false;
  }

  // Get display information
  DDCA_Display_Info_List* dlist = nullptr;
  rc = ddca_get_display_info_list2(false, &dlist);
  if (rc != 0) {
    std::cerr << "ddca_get_display_info_list2() failed: " << ddca_rc_name(rc)
              << std::endl;
    return false;
  }

  if (dlist->ct == 0) {
    std::cerr << "No DDC capable displays found" << std::endl;
    ddca_free_display_info_list(dlist);
    return false;
  }

  std::cout << "Found " << dlist->ct << " DDC capable display(s)" << std::endl;

  // Open all displays
  for (int i = 0; i < dlist->ct; i++) {
    DDCA_Display_Ref dref = dlist->info[i].dref;
    DDCA_Display_Handle dh = nullptr;

    std::cout << "Opening display " << (i + 1) << ": "
              << dlist->info[i].model_name << std::endl;

    rc = ddca_open_display2(dref, false, &dh);
    if (rc != 0) {
      std::cerr << "  Failed to open display: " << ddca_rc_name(rc)
                << std::endl;
      continue;
    }

    display_handles.push_back(dh);
    std::cout << "  Successfully opened" << std::endl;
  }

  ddca_free_display_info_list(dlist);

  if (display_handles.empty()) {
    std::cerr << "Failed to open any displays" << std::endl;
    return false;
  }

  std::cout << "Successfully initialized DDC connection to "
            << display_handles.size() << " display(s)" << std::endl;
  return true;
}

int BrightnessController::getCurrentBrightness() {
  if (display_handles.empty()) return -1;

  // Get brightness from first display (for display purposes)
  DDCA_Non_Table_Vcp_Value valrec;
  DDCA_Status rc =
      ddca_get_non_table_vcp_value(display_handles[0], VCP_BRIGHTNESS, &valrec);

  if (rc != 0) {
    std::cerr << "Failed to get brightness: " << ddca_rc_name(rc) << std::endl;
    return -1;
  }

  return valrec.sh << 8 | valrec.sl;  // Current value
}

bool BrightnessController::setBrightness(int value) {
  if (display_handles.empty()) return false;

  // Clamp value between 0 and 100
  if (value < 0) value = 0;
  if (value > 100) value = 100;

  bool success = true;

  // Set brightness on all displays
  for (size_t i = 0; i < display_handles.size(); i++) {
    DDCA_Status rc = ddca_set_non_table_vcp_value(display_handles[i],
                                                  VCP_BRIGHTNESS, 0, value);

    if (rc != 0) {
      std::cerr << "Failed to set brightness on display " << (i + 1) << ": "
                << ddca_rc_name(rc) << std::endl;
      success = false;
    }
  }

  return success;
}

void BrightnessController::onBrightnessUp() {
  std::cout << "🔆 Brightness UP key pressed!" << std::endl;

  if (!display_handles.empty()) {
    int current = getCurrentBrightness();
    if (current >= 0) {
      int new_brightness = current + BRIGHTNESS_STEP;
      if (setBrightness(new_brightness)) {
        std::cout << "   All displays: " << current << "% → "
                  << (new_brightness > 100 ? 100 : new_brightness) << "%"
                  << std::endl;
      }
    }
  }
}

void BrightnessController::onBrightnessDown() {
  std::cout << "🔅 Brightness DOWN key pressed!" << std::endl;

  if (!display_handles.empty()) {
    int current = getCurrentBrightness();
    if (current >= 0) {
      int new_brightness = current - BRIGHTNESS_STEP;
      if (setBrightness(new_brightness)) {
        std::cout << "   All displays: " << current << "% → "
                  << (new_brightness < 0 ? 0 : new_brightness) << "%"
                  << std::endl;
      }
    }
  }
}
