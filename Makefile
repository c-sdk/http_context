CC ?= cc
CFLAGS ?= -Wall -Werror -std=c17 -I. -I./deps/pagesize -I./deps/arena -I./deps/string_map -I./deps/http_types

TEST_BIN := t/http_context_test
TEST_SRC := t/http_context_test.c http_context.c \
            deps/arena/arena.c deps/string_map/string_map.c deps/pagesize/pagesize.c

.PHONY: all test run clean

all: test

$(TEST_BIN): $(TEST_SRC) http_context.h
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC)

test: $(TEST_BIN)
	./$(TEST_BIN)

run: test

clean:
	rm -f $(TEST_BIN)
