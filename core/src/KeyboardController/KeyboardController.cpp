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
    int fd = open(device.c_str(), O_RDONLY);
    if (fd < 0) {
      std::cerr << "Cannot open " << device << ": " << strerror(errno)
                << std::endl;
      std::cerr << "Try running with sudo or add user to 'input' group"
                << std::endl;
      continue;
    }
    fds.push_back(fd);
  }

  if (fds.empty()) {
    return false;
  }

  return true;
}

void KeyboardController::monitor(KBindings bindings) {
  fd_set readfds;
  struct input_event ev;

  while (true) {
    FD_ZERO(&readfds);
    int max_fd = 0;

    for (int fd : fds) {
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

    for (int fd : fds) {
      if (FD_ISSET(fd, &readfds)) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n == sizeof(ev)) {
          if (ev.type == EV_KEY && ev.value == 1) {
            for (Binding binding : bindings) {
              if (ev.code == binding.key) {
                binding.callback();
              }
            }
          }
        }
      }
    }
  }
}
