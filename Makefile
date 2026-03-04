CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -Iinclude
LDFLAGS  = -pthread

SRC_DIR  = src
OBJ_DIR  = obj
SRCS     = $(wildcard $(SRC_DIR)/*.cpp)
OBJS     = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET   = redis

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean