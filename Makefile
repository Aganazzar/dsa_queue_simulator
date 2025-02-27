# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -I./include -I/opt/homebrew/include
LDFLAGS = $(shell sdl2-config --libs) -lSDL2 -lSDL2_ttf -lpthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
RUN = src/simulator.c 

# Object files
SIMULATOR_OBJECTS = $(RUN:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Executables
SIMULATOR = SIMULATOR


# Default target
all: setup $(SIMULATOR) 

# Create build directory
setup:
	@mkdir -p $(BUILD_DIR)

# Build SIMULATOR
$(SIMULATOR): $(SIMULATOR_OBJECTS)
	$(CC) $(SIMULATOR_OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(SIMULATOR) 

.PHONY: all setup clean