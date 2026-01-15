CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lsqlite3 -lpthread

TARGET = candidate_scoring
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS) templates.h httplib.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
