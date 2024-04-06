CC=gcc
CFLAGS=-g3 -Wall -Wextra -Werror -fsanitize=address,undefined

all: bin/shell

bin/shell: shell.c shell.h
	$(CC) $(CFLAGS) shell.c -o bin/shell

clean:
	rm -f bin/*

.PHONY: clean