CC = g++
CFLAGS = -c -Wall
LDFLAGS =

# List of sources:
SOURCES = statmon.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Name of executable target:
EXECUTABLE = statmon

# add flags for libnl libraries
CFLAGS += -I/usr/include/libnl3 
LDFLAGS += -lnl-route-3 -lnl-3

.PHONY: all debug

all: $(SOURCES) $(EXECUTABLE)

debug: CFLAGS += -DDEBUG -g
debug: all

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJECTS) $(EXECUTABLE)
