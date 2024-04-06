CC=gcc
CFLAGS=-g3 -Wall -Wextra -Werror -fsanitize=address,undefined

all: shell

shell: shell.c shell.h
	$(CC) $(CFLAGS) -o shell shell.c

clean:
	rm -f bin/*

.PHONY: clean