# Compiler and flags
CXX := clang++
CXXFLAGS := -std=c++17 -O2

# Directories
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
PROJ_DIR := .

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Main source file
MAIN_SRC := engine.cpp

# Executable name
TARGET := game_engine_linux

# Include and library directories
INC_DIRS := -I$(PROJ_DIR) -I$(PROJ_DIR)/include -I$(PROJ_DIR)/SDL2 -I$(PROJ_DIR)/SDL2_image -I$(PROJ_DIR)/SDL2_ttf -I$(PROJ_DIR)/SDL2_mixer

LIB_DIRS := -L/usr/local/lib -L$(PROJ_DIR) -L$(PROJ_DIR)/SDL2 -L$(PROJ_DIR)/SDL2_image -L$(PROJ_DIR)/SDL2_ttf -L$(PROJ_DIR)/SDL2_mixer

# Libraries to link against
LIBS := -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer

# Targets and rules
$(TARGET): $(OBJS) $(BUILD_DIR)/engine.o
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INC_DIRS) $(LIB_DIRS) $^ -o $@ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INC_DIRS) -c $< -o $@

$(BUILD_DIR)/engine.o: $(MAIN_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INC_DIRS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean
