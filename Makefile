CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

.PHONY: all test run clean
all: test

mysh: mysh.c
	$(CC) $(CFLAGS) -o $@ $<

test: mysh
	sh tests.sh

run: mysh
	./mysh

clean:
	rm -f mysh
