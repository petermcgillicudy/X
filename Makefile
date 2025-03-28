# Compiler and flags
CXX = g++
CXXFLAGS_DEBUG = -g -Wall -Wextra -std=c++17
CXXFLAGS_RELEASE = -O3 -Wall -Wextra -std=c++17 -DNDEBUG

# Directories
SRCDIR = src
BINDIR = bin

# Source files and target
SRCS = $(SRCDIR)/main.cpp $(SRCDIR)/controls.cpp
HEADERS = $(wildcard $(SRCDIR)/*.hpp)
TARGET = $(BINDIR)/x_debug
TARGET_RELEASE = $(BINDIR)/x

# Create directories if they don't exist
$(shell mkdir -p $(BINDIR))

# Default target
all: debug

# Debug build
debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: $(TARGET)

# Release build
release: CXXFLAGS = $(CXXFLAGS_RELEASE)
release: $(TARGET_RELEASE)

# Build the debug target
$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS_DEBUG) $^ -o $@

# Build the release target
$(TARGET_RELEASE): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS_RELEASE) $^ -o $@

# Clean up
clean:
	rm -rf $(BINDIR)/*

# Run the debug version
run: debug
	$(TARGET)

# Run the release version
run_release: release
	$(TARGET_RELEASE)

.PHONY: all debug release clean run run_release