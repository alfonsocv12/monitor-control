#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
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
  // todo: change name of fds
  std::vector<int> fileDescriptors;
  std::vector<std::string> findKeyboardDevices();
  std::vector<KeyPress> readPressedKeys();
  void discardPendingEvents();

 public:
  bool init();
  void monitor(KBindings bindings);
};
