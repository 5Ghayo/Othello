CXX = clang++
CXXFLAGS = -std=c++17 -Wall -O2
INCLUDES = -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib
LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit

SRCS = main.cpp board.cpp ai.cpp game.cpp replay.cpp network.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = othello

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
