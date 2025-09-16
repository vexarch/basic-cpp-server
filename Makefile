CXX = g++
CXXFLAGS = -Iinclude -Wall -Wextra -std=c++17 -fPIC
LDFLAGS = -shared
LIBS = -lssl -lcrypto

TARGET = libcpphttp.so

SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
EXAMPLE = basic-server

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)


all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(LIBS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

example: $(OBJECTS)
	$(CXX) example/main.cpp -Iinclude $^ $(LIBS) -o basic-server

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(EXAMPLE)

rebuild: clean all

.PHONY: all clean rebuild example
