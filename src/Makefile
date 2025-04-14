CXX = g++
# Use C++11 or later for features like enum class, unique_ptr, stoul, etc.
CXXFLAGS = -std=c++20 -Wall -Wextra -g # -O2 for optimization, -g for debugging
LDFLAGS =

# List of source files
SRCS = main.cpp simulator.cpp core.cpp cache.cpp bus.cpp stats.cpp
# List of object files
OBJS = $(SRCS:.cpp=.o)
# Executable name
TARGET = L1simulate

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule to compile .cpp files into .o files
# Added explicit dependency on definitions.h for all objects
%.o: %.cpp *.h definitions.h Makefile
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)