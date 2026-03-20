CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lddcutil
TARGET = ./build/brightness_control
SRC = ./src/brightness_control.cpp ./src/KeyboardController/KeyboardController.cpp ./src/BrightnessController/BrightnessController.cpp

all: $(TARGET)

compile:
	mkdir -p ./build
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo cp core/monitor-control.service /etc/systemd/system/
	@echo "Installed to /usr/local/bin/$(TARGET)"

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

run: $(TARGET)
	sudo ./$(TARGET)

format:
	find src/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i -style=Google

.PHONY: all clean install uninstall run

