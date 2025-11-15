#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// typedef void (*KActionCallback)();
typedef std::function<void()> KActionCallback;

struct Binding {
  int key;
  KActionCallback callback;
};

typedef std::vector<Binding> KBindings;

class KeyboardController {
 private:
  // todo: change name of fds
  std::vector<int> fds;
  std::vector<std::string> findKeyboardDevices();

 public:
  bool init();
  void monitor(KBindings bindings);
};
