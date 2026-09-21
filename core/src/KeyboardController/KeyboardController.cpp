#include "KeyboardController.h"

std::vector<std::string> KeyboardController::findKeyboardDevices() {
  std::vector<std::string> devices;
  DIR* dir = opendir("/dev/input");
  if (!dir) {
    std::cerr << "Cannot open /dev/input" << std::endl;
    return devices;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strncmp(entry->d_name, "event", 5) == 0) {
      std::string path = "/dev/input/" + std::string(entry->d_name);
      int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
      if (fd >= 0) {
        char name[256] = "Unknown";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        unsigned long evbit = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);

        if (evbit & (1 << EV_KEY)) {
          devices.push_back(path);
          std::cout << "Found input device: " << name << " (" << path << ")"
                    << std::endl;
        }
        close(fd);
      }
    }
  }
  closedir(dir);
  return devices;
};

bool KeyboardController::init() {
  auto devices = findKeyboardDevices();
  if (devices.empty()) {
    std::cerr << "No input devices found" << std::endl;
    return false;
  }

  for (const auto& device : devices) {
    // Non-blocking so pending events can be drained without stalling.
    int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      std::cerr << "Cannot open " << device << ": " << strerror(errno)
                << std::endl;
      std::cerr << "Try running with sudo or add user to 'input' group"
                << std::endl;
      continue;
    }
    fileDescriptors.push_back(fd);
  }

  if (fileDescriptors.empty()) {
    return false;
  }

  return true;
}

std::vector<KeyPress> KeyboardController::readPressedKeys() {
  std::vector<KeyPress> presses;
  struct input_event ev;

  // Drain every queued event and tally repeats, so a fast burst of presses
  // becomes one invocation carrying the count rather than one action per
  // event. Callbacks here can block, and dispatching each event separately
  // would keep firing long after the user stopped pressing.
  for (int fd : fileDescriptors) {
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
      if (ev.type != EV_KEY || ev.value != 1) continue;

      auto match = std::find_if(
          presses.begin(), presses.end(),
          [&ev](const KeyPress& press) { return press.key == (int)ev.code; });

      if (match == presses.end()) {
        presses.push_back(KeyPress{(int)ev.code, 1});
      } else {
        match->count++;
      }
    }
  }

  return presses;
}

void KeyboardController::discardPendingEvents() {
  struct input_event ev;

  for (int fd : fileDescriptors) {
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
    }
  }
}

void KeyboardController::monitor(KBindings bindings) {
  fd_set readfds;

  while (true) {
    FD_ZERO(&readfds);
    int max_fd = 0;

    for (int fd : fileDescriptors) {
      FD_SET(fd, &readfds);
      if (fd > max_fd) max_fd = fd;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
    if (ret < 0) {
      std::cerr << "Select error: " << strerror(errno) << std::endl;
      break;
    }

    for (const KeyPress& press : readPressedKeys()) {
      for (const Binding& binding : bindings) {
        if (press.key == binding.key) {
          binding.callback(press.count);
        }
      }
    }

    // Presses that arrived while the callbacks ran are stale. Replaying them
    // is what turns one slow DDC call into a burst aimed at a display that is
    // still coming back up.
    discardPendingEvents();
  }
}
