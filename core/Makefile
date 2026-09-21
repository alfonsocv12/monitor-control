CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lddcutil
TARGET = ./build/main
# Must match ExecStart in monitor-control.service
INSTALL_PATH = /usr/local/bin/brightness_control
SRC = ./src/main.cpp ./src/KeyboardController/KeyboardController.cpp ./src/BrightnessController/BrightnessController.cpp

all: $(TARGET)

compile:
	mkdir -p ./build
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) $(INSTALL_PATH)
	@echo "Installed to $(INSTALL_PATH)"

uninstall:
	sudo rm -f $(INSTALL_PATH)

run: $(TARGET)
	sudo ./$(TARGET)

format:
	find src/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=Google

.PHONY: all clean install uninstall run
