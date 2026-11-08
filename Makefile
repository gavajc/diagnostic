# Target executable name modified to avoid python daemon collision
TARGET = diagnostic

# Compilation flags with C++17 enforcement and optimization overrides
CXXFLAGS += -O3 -std=c++17 -Wall -I. -I./includes
LIBS += -luci -lubox -pthread

# Source files matching your project layout
SRCS = main.cpp configmanager.cpp httpclient.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o
