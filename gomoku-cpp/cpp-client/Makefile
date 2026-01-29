# Gomoku Client Makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2
TARGET = gomoku_client
SRC = client.cpp

all: $(TARGET)

$(TARGET): $(SRC) ../cpp-server/protocol.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Connect to a specific server
connect: $(TARGET)
	@read -p "Server IP [127.0.0.1]: " ip; \
	read -p "Port [8888]: " port; \
	./$(TARGET) $${ip:-127.0.0.1} $${port:-8888}

.PHONY: all clean debug run connect
