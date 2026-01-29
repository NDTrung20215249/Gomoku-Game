# Gomoku Server Makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2
TARGET = gomoku_server
SRC = server.cpp

all: $(TARGET)

$(TARGET): $(SRC) protocol.h database.h game_logic.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)

clean:
	rm -f $(TARGET)
	
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean debug run
