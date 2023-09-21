CC = g++
CFLAGS = -Wall -std=c++11
LDFLAGS =

TARGET = device_simulator

all: $(TARGET)

$(TARGET): device_simulator.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) device_simulator.cpp

clean:
	rm -f $(TARGET)

