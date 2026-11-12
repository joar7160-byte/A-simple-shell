CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

.PHONY: all test run clean
all: test

mysh: mysh.c
	$(CC) $(CFLAGS) -o mysh mysh.c

test: mysh
	@echo "PASS: compilation"
	@result="$$(printf 'pwd\nexit\n' | ./mysh)" && [ "$$result" = "$$(pwd)" ] \
		&& echo "PASS: pwd and batch prompts" || { echo "FAIL: pwd or batch prompts"; exit 1; }
	@result="$$(printf 'exit\npwd\n' | ./mysh)" && [ -z "$$result" ] \
		&& echo "PASS: exit" || { echo "FAIL: exit"; exit 1; }
	@result="$$(printf 'pwd' | ./mysh)" && [ "$$result" = "$$(pwd)" ] \
		&& echo "PASS: input without newline" || { echo "FAIL: input without newline"; exit 1; }

run: mysh
	./mysh

clean:
	rm -f mysh

