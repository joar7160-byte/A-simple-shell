#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE 4096

int main(int argc, char *argv[]){
    char line[MAX_LINE];
    int input_fd = STDIN_FILENO;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [file]\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc == 2) {
        input_fd = open(argv[1], O_RDONLY);

        if (input_fd == -1) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
    }

    int interactive = argc == 1 && isatty(STDIN_FILENO);
     while (1) {
        if (interactive) {
            printf("mysh$ ");
            fflush(stdout);
        }

        char ch;
        int ind = 0;
        int too_long = 0;
        ssize_t bytes;

        while ((bytes = read(input_fd, &ch, 1)) != 0) {
            if (bytes < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (ch == '\n') {
                break;
            }
            if (ind < MAX_LINE - 1) {
                line[ind++] = ch;
            } else {
                too_long = 1;
            }
        }
        if (bytes < 0) {
            perror("read");
            break;
        }
        if (bytes == 0 && ind == 0) {
            break;
        }
        if (too_long) {
            fprintf(stderr, "mysh: command line too long\n");
            continue;
        }
        line[ind] = '\0';
        if (strcmp(line, "exit") == 0) {
            break;
        }
        if (strcmp(line, "pwd") == 0) {
            char cwd[MAX_LINE];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                perror("pwd");
            } else {
                printf("%s\n", cwd);
            }
        }
    }
    if (input_fd != STDIN_FILENO) {
        close(input_fd);
    }
    return EXIT_SUCCESS;
}