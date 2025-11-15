LIB_NAME = BrightnessController
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
OUTPUT_FOLDER = ./build
TARGET = $(OUTPUT_FOLDER)/kc.o
SRC = ./*.cpp

compile:
	mkdir -p $(OUTPUT_FOLDER)
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(TARGET)
	ar rcs ./build/$(LIB_NAME).a ./build/kc.o
