# Compiler and options
CXX = g++
CXXFLAGS = -Wall -O2 -Iinclude
LIBS = -lfreeglut -lopengl32 -lglu32
LDFLAGS = -LC:/msys64/mingw64/lib

# Directories
SRC_DIR = src
BUILD_DIR = build

# Source and object files
SRCS = main.cpp viewport.cpp xmlload.cpp lodepng.cpp tinyxml2.cpp objects.cpp materials.cpp
OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Target executable
TARGET = raytracer.exe

# Default rule
all: $(TARGET)

# Link step
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

# Compile step
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Make sure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean
clean:
	cmd /c del /Q *.o $(TARGET)

