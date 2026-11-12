CS 214 Project III: A Simple Shell

Kevin Garcia - kag408
Joarder Rashid - jmr688

Building and running
--------------------
Run make to compile mysh.c and run the basic checks
Run ./mysh to enter commands in the terminal
Run ./mysh filename to read commands from a file
You can also pipe input into the program:

    printf 'pwd\nexit\n' | ./mysh

make run starts the shell, make test repeats the basic checks, and
make clean removes the executable

Current implementation
----------------------
The program uses read() to read one character at a time. It processes
one line before reading the next command. It accepts input from the
terminal, a pipe, or a file given on the command line.

The current built-ins are pwd and exit. These must be entered without
extra arguments or surrounding spaces. The program prints a temporary
mysh$ prompt in interactive mode and no prompts in batch mode.

Testing
-------
The Makefile checks these cases and prints PASS or FAIL:

- pwd followed by exit: prints the working directory with no prompt.
- exit followed by pwd: exits without running pwd.
- pwd without a final newline: still runs the command.

The tests stop at the first failure. These checks cover the current
input handling, not the full assignment.

Additional checks to run:

- Put pwd and exit in a file and pass its filename to ./mysh
  the output should match pwd, without a prompt
- Give ./mysh a filename that does not exist. It should print an error
  and return a nonzero exit status
- Run ./mysh interactively and try pwd, an empty line, exit, and Ctrl+D
- Enter a line longer than 4095 characters. It should report that the
  line is too long instead of executing a shortened command

mysh_tests.txt has inputs for the remaining features. Its comments
explain the expected output. Run it from a fresh directory because
it creates test files. The current version cannot pass all of those
tests yet. Test results should be recorded after checking the output.

Remaining work
--------------
The shell still needs command parsing, comments, cd, which, external
program execution, wildcard expansion, redirection, and pipelines.
It also needs the required directory prompt, welcome/goodbye messages,
interactive failure messages, and batch-mode child input handling.
exit with arguments or in a pipeline is not implemented.

Update this README as those features are completed and tested.
