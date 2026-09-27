#include "KeyboardController.h"

bool KeyboardController::isKeyDevice(const std::string& path,
                                     std::string& name) {
  int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) return false;

  char buffer[256] = "Unknown";
  ioctl(fd, EVIOCGNAME(sizeof(buffer)), buffer);
  name = buffer;

  unsigned long evbit = 0;
  ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);
  close(fd);

  return evbit & (1 << EV_KEY);
}

void KeyboardController::addDevice(const std::string& path) {
  for (const InputDevice& device : devices) {
    if (device.path == path) return;
  }

  std::string name;
  if (!isKeyDevice(path, name)) return;

  // Non-blocking so pending events can be drained without stalling.
  int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::cerr << "Cannot open " << path << ": " << strerror(errno)
              << std::endl;
    std::cerr << "Try running with sudo or add user to 'input' group"
              << std::endl;
    return;
  }

  devices.push_back(InputDevice{fd, path});
  std::cout << "Found input device: " << name << " (" << path << ")"
            << std::endl;
}

bool KeyboardController::init() {
  // Start watching before the scan so nothing plugged in between is missed.
  hotplugFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (hotplugFd >= 0 &&
      inotify_add_watch(hotplugFd, "/dev/input", IN_CREATE | IN_ATTRIB) < 0) {
    close(hotplugFd);
    hotplugFd = -1;
  }
  if (hotplugFd < 0) {
    std::cerr << "Input hotplug detection unavailable: " << strerror(errno)
              << std::endl;
  }

  DIR* dir = opendir("/dev/input");
  if (!dir) {
    std::cerr << "Cannot open /dev/input" << std::endl;
    return false;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strncmp(entry->d_name, "event", 5) == 0) {
      addDevice("/dev/input/" + std::string(entry->d_name));
    }
  }
  closedir(dir);

  if (devices.empty()) {
    std::cerr << "No input devices found" << std::endl;
    return false;
  }

  return true;
}

void KeyboardController::addHotpluggedDevices() {
  alignas(inotify_event) char buffer[4096];
  ssize_t length;

  while ((length = read(hotplugFd, buffer, sizeof(buffer))) > 0) {
    for (char* ptr = buffer; ptr < buffer + length;) {
      auto* event = reinterpret_cast<inotify_event*>(ptr);
      if (event->len > 0 && strncmp(event->name, "event", 5) == 0) {
        addDevice("/dev/input/" + std::string(event->name));
      }
      ptr += sizeof(inotify_event) + event->len;
    }
  }
}

void KeyboardController::readEvents(
    const std::function<void(const input_event&)>& onEvent) {
  struct input_event ev;

  for (auto it = devices.begin(); it != devices.end();) {
    ssize_t n;
    while ((n = read(it->fd, &ev, sizeof(ev))) == sizeof(ev)) {
      onEvent(ev);
    }

    // An unplugged device stays readable forever and fails every read with
    // ENODEV. Keeping it would turn select() into a busy loop.
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
      std::cerr << "Lost input device " << it->path << ": " << strerror(errno)
                << std::endl;
      close(it->fd);
      it = devices.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<KeyPress> KeyboardController::readPressedKeys() {
  std::vector<KeyPress> presses;

  // Drain every queued event and tally repeats, so a fast burst of presses
  // becomes one invocation carrying the count rather than one action per
  // event. Callbacks here can block, and dispatching each event separately
  // would keep firing long after the user stopped pressing.
  readEvents([&presses](const input_event& ev) {
    if (ev.type != EV_KEY || ev.value != 1) return;

    auto match = std::find_if(
        presses.begin(), presses.end(),
        [&ev](const KeyPress& press) { return press.key == (int)ev.code; });

    if (match == presses.end()) {
      presses.push_back(KeyPress{(int)ev.code, 1});
    } else {
      match->count++;
    }
  });

  return presses;
}

void KeyboardController::discardPendingEvents() {
  readEvents([](const input_event&) {});
}

void KeyboardController::monitor(KBindings bindings) {
  fd_set readfds;

  while (true) {
    FD_ZERO(&readfds);
    int max_fd = 0;

    for (const InputDevice& device : devices) {
      FD_SET(device.fd, &readfds);
      if (device.fd > max_fd) max_fd = device.fd;
    }

    if (hotplugFd >= 0) {
      FD_SET(hotplugFd, &readfds);
      if (hotplugFd > max_fd) max_fd = hotplugFd;
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

    // After the reads, so a dead device sharing a path with a replugged one
    // has already been dropped.
    if (hotplugFd >= 0 && FD_ISSET(hotplugFd, &readfds)) {
      addHotpluggedDevices();
    }
  }
}
