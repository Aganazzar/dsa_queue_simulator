# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -I./include -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -lpthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
SIMULATOR_SOURCES = src/simulator.c 
GENERATOR_SOURCES = src/traffic_generator.c 

# Object files
SIMULATOR_OBJECTS = $(SIMULATOR_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
GENERATOR_OBJECTS = $(GENERATOR_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Executables
SIMULATOR = simulator
GENERATOR = generator

# Default target
all: setup $(SIMULATOR) $(GENERATOR)

# Create build directory
setup:
	@mkdir -p $(BUILD_DIR)

# Build simulator
$(SIMULATOR): $(SIMULATOR_OBJECTS)
	$(CC) $(SIMULATOR_OBJECTS) -o $@ $(LDFLAGS)

# Build generator
$(GENERATOR): $(GENERATOR_OBJECTS)
	$(CC) $(GENERATOR_OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(SIMULATOR) $(GENERATOR)

.PHONY: all setup clean