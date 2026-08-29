CC ?= cc

CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

TARGET := build/6502emu
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:src/%.c=build/%.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJECTS) | build
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c src/6502.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) -r build

-include $(DEPS)
