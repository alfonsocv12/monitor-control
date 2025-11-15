CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
OUTPUT_FOLDER = ./build
TARGET = $(OUTPUT_FOLDER)/kc.o
SRC = ./KeyboardController.cpp

compile:
	mkdir -p $(OUTPUT_FOLDER)
	$(CXX) $(CXXFLAGS) -c $(SRC) -o $(TARGET)
	ar rcs ./build/keyboardlib.a ./build/kc.o
