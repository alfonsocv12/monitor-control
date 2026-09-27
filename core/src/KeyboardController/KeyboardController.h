#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Receives how many presses were coalesced into this invocation.
typedef std::function<void(int)> KActionCallback;

struct Binding {
  int key;
  KActionCallback callback;
};

typedef std::vector<Binding> KBindings;

struct KeyPress {
  int key;
  int count;
};

class KeyboardController {
 private:
  struct InputDevice {
    int fd;
    std::string path;
  };

  std::vector<InputDevice> devices;
  // Watches /dev/input so devices plugged in after startup are picked up.
  // IN_ATTRIB too: udev grants group access only after the node is created.
  int hotplugFd = -1;
  bool isKeyDevice(const std::string& path, std::string& name);
  void addDevice(const std::string& path);
  void addHotpluggedDevices();
  void readEvents(const std::function<void(const input_event&)>& onEvent);
  std::vector<KeyPress> readPressedKeys();
  void discardPendingEvents();

 public:
  bool init();
  void monitor(KBindings bindings);
};
