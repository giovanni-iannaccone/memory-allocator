CXX = g++
CXXFLAGS = -Wall -Wextra -Wpedantic -std=c++17 -Iinclude -g

SRC_DIR = src
TEST_DIR = test
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

SRCS = $(SRC_DIR)/allocator.cpp $(TEST_DIR)/test.cpp
OBJS = $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

TARGET = $(BIN_DIR)/test_allocator

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean