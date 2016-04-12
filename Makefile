CC = g++
CFLAGS = -c -Wall -std=c++11
LDFLAGS =

# List of sources:
SOURCES = statmon.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Name of executable target:
EXECUTABLE = statmon

# add pkg-config includes for libnl libraries
CFLAGS += `pkg-config --cflags libnl-3.0`
CFLAGS += `pkg-config --cflags libnl-route-3.0`
LDFLAGS += `pkg-config --libs libnl-3.0`
LDFLAGS += `pkg-config --libs libnl-route-3.0`

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
