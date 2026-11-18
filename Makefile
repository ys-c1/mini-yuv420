CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter
LDLIBS ?= -lm

BUILD := build
SRC := src/yuv.c
TEST := $(BUILD)/test_yuv

.PHONY: all test clean demo

all: $(TEST)

$(BUILD):
	mkdir -p $(BUILD)

$(TEST): $(SRC) tests/test_yuv.c src/yuv.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_yuv.c $(LDLIBS)

test: $(TEST)
	./$(TEST)

demo: | $(BUILD)
	$(CC) $(CFLAGS) -o $(BUILD)/yuv_demo src/yuv.c tools/yuv_demo.c $(LDLIBS)
	./$(BUILD)/yuv_demo

clean:
	rm -rf $(BUILD)
