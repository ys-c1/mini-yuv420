CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter
LDLIBS ?= -lm

BUILD := build
TEST := $(BUILD)/test_all

.PHONY: all test clean demo

all: $(TEST)

$(BUILD):
	mkdir -p $(BUILD)

$(TEST): $(wildcard src/*.c) tests/*.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(wildcard src/*.c) tests/*.c $(LDLIBS)

test: $(TEST)
	./$(TEST)

demo: | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/demo $(wildcard src/*.c) $(wildcard tools/*.c) $(LDLIBS)
	./$(BUILD)/demo

clean:
	rm -rf $(BUILD)
