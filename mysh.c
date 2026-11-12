#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <glob.h>
#include <sys/wait.h>

typedef struct {
    char **args;
    size_t count;
    char *input;
    char *output;
    int append;
} Command;

typedef struct {
    int code;
    int signal;
} JobResult;

static const char *home_directory;
static void *checked_realloc(void *memory, size_t bytes){
    void *result = realloc(memory, bytes);
    if (!result) {
        perror("realloc");
        exit(EXIT_SUCCESS);
    }
    return result;
}

static char *copy_string(const char *text){
    char *copy = checked_realloc(NULL, strlen(text) + 1);
    strcpy(copy, text);
    return copy;
}

static char *current_directory(void){
    size_t capacity = 256;
    char *path = NULL;
    for (;;) {
        path = checked_realloc(path, capacity);
        if (getcwd(path, capacity)) return path;
        if (errno != ERANGE) { free(path);
            return NULL;
        }
        capacity *= 2;
    }
}

static void print_prompt(void){
    char *cwd = current_directory();
    if (!cwd) {
        perror("getcwd");
        printf("mysh$ ");
    } else {
        size_t length = home_directory ? strlen(home_directory) : 0;
        while (length > 1 && home_directory[length - 1] == '/') length--;
        int in_home = length && !strncmp(cwd, home_directory, length) &&
            (cwd[length] == '\0' || cwd[length] == '/' || length == 1);
        if (in_home) {
            const char *separator = length == 1 && cwd[1] ? "/" : "";
            printf("~%s%s$ ", separator, cwd + length);
        } else {
            printf("%s$ ", cwd);
        }
        free(cwd);
    }
    fflush(stdout);
}

// Stop at the newline so the command runs before another read
static char *read_line(int fd){
    size_t length = 0;
    size_t capacity = 128;
    char *line = checked_realloc(NULL, capacity);
    for (;;) {
        char ch;
        ssize_t bytes = read(fd, &ch, 1);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            free(line);
            return NULL;
        }
        if (!bytes || ch == '\n') {
            if (!bytes && !length) {
                free(line);
                return NULL;
            }
            line[length] = '\0';
            return line;
        }
        if (length + 1 == capacity) {
            capacity *= 2;
            line = checked_realloc(line, capacity);
        }
        line[length++] = ch;
    }
}

static int is_operator(const char *token){
    return !strcmp(token, "|") || !strcmp(token, "<") || !strcmp(token, ">") || !strcmp(token, ">>");
}

static char **tokenize(char *line, size_t *count)
{
    char **tokens = NULL;
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    *count = 0;
    char *p = line;
    while (*p) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        char *start = p;
        if (strchr("<>|", *p)) {
            if (*p++ == '>' && *p == '>') p++;
        } else {
            while (*p && !isspace((unsigned char)*p) && !strchr("<>|", *p))
                p++;
        }
        size_t length = (size_t)(p - start);
        char *token = checked_realloc(NULL, length + 1);
        memcpy(token, start, length);
        token[length] = '\0';
        tokens = checked_realloc(tokens, (*count + 1) * sizeof(*tokens));
        tokens[(*count)++] = token;
    }
    return tokens;
}

static void add_argument(Command *command, const char *text){
    size_t bytes = (command->count + 2) * sizeof(*command->args);
    command->args = checked_realloc(command->args, bytes);
    command->args[command->count++] = copy_string(text);
    command->args[command->count] = NULL;
}

static int expand_argument(Command *command, const char *text){
    if (!strchr(text, '*')) {
        add_argument(command, text);
        return 0;
    }
    //P3 only gives '*' a special meaning. Escape other glob syntax.
    char *pattern = checked_realloc(NULL, 2 * strlen(text) + 1);
    size_t length = 0;
    for (size_t i = 0; text[i]; i++) {
        if (strchr("?[]\\", text[i]))
            pattern[length++] = '\\';
        pattern[length++] = text[i];
    }
    pattern[length] = '\0';
    glob_t matches = {0};
    int result = glob(pattern, 0, NULL, &matches);
    free(pattern);
    if (result == GLOB_NOMATCH) {
        add_argument(command, text);
    } else if (result == 0) {
        for (size_t i = 0; i < matches.gl_pathc; i++)
            add_argument(command, matches.gl_pathv[i]);
    } else {
        fprintf(stderr, "mysh: wildcard expansion failed: %s\n", text);
    }
    globfree(&matches);
    return result != 0 && result != GLOB_NOMATCH;
}

static void free_commands(Command *commands, size_t count){
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < commands[i].count; j++)
            free(commands[i].args[j]);
        free(commands[i].args);
        free(commands[i].input);
        free(commands[i].output);
    }
    free(commands);
}

static int parse_commands(char **tokens, size_t token_count, Command *commands, size_t *count){
    *count = 1;
    for (size_t i = 0; i < token_count; i++) {
        Command *command = &commands[*count - 1];
        if (!strcmp(tokens[i], "|")) {
            if (!command->count)
                goto syntax_error;
            (*count)++;
        } else if (is_operator(tokens[i])) {
            int input = !strcmp(tokens[i], "<");
            int append = !strcmp(tokens[i], ">>");
            if (++i == token_count || is_operator(tokens[i]))
                goto syntax_error;
            if (input) {
                free(command->input);
                command->input = copy_string(tokens[i]);
            } else {
                free(command->output);
                command->output = copy_string(tokens[i]);
                command->append = append;
            }
        } else if (expand_argument(command, tokens[i])) {
            return -1;
        }
    }
    if (!commands[*count - 1].count) goto syntax_error;
    return 0;
syntax_error:
    fprintf(stderr, "mysh: syntax error\n");
    return -1;
}

static int is_builtin(const char *name){
    return !strcmp(name, "cd") || !strcmp(name, "pwd") || !strcmp(name, "which") || !strcmp(name, "exit");
}

static char *find_executable(const char *name){
    if (strchr(name, '/'))
        return access(name, X_OK) == 0 ? copy_string(name) : NULL;
    const char *directories[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    size_t count = sizeof(directories) / sizeof(directories[0]);
    for (size_t i = 0; i < count; i++) {
        size_t length = strlen(directories[i]) + strlen(name) + 2;
        char *path = checked_realloc(NULL, length);
        snprintf(path, length, "%s/%s", directories[i], name);
        if (access(path, X_OK) == 0)return path;
        free(path);
    }
    return NULL;
}

static int run_builtin(Command *command){
    char **args = command->args;
    if (!strcmp(args[0], "exit"))
        return 0;
    if (!strcmp(args[0], "cd")) {
        if (command->count > 2) {
            fprintf(stderr, "cd: too many arguments\n");
            return 1;
        }
        const char *target = command->count == 2 ? args[1] : home_directory;
        if (!target) {
            fprintf(stderr, "cd: HOME is not set\n");
            return 1;
        }
        if (chdir(target) < 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }
    if (!strcmp(args[0], "pwd")) {
        if (command->count != 1) {
            fprintf(stderr, "pwd: too many arguments\n");
            return 1;
        }
        char *cwd = current_directory();
        if (!cwd) {
            perror("pwd");
            return 1;
        }
        int result = puts(cwd);
        free(cwd);
        return result == EOF;
    }
    if (command->count == 1) {
        fprintf(stderr, "which: expected an argument\n");
        return 1;
    }
    int failed = 0;
    for (size_t i = 1; i < command->count; i++) {
        char *path = find_executable(args[i]);
        if (path) {
            if (puts(path) == EOF)
                failed = 1;
            free(path);
        } else {
            fprintf(stderr, "which: %s: not found\n", args[i]);
            failed = 1;
        }
    }
    return failed;
}

static int redirect_fd(int fd, int target){
    if (fd == target)
        return 0;
    int result = dup2(fd, target);
    close(fd);
    if (result < 0) perror("dup2");
    return result < 0 ? -1 : 0;
}

static int apply_redirection(Command *command){
    if (command->input) {
        int fd = open(command->input, O_RDONLY);
        if (fd < 0) {
            perror(command->input);
            return -1;
        }
        if (redirect_fd(fd, STDIN_FILENO) < 0) return -1;
    }
    if (command->output) {
        int flags = O_WRONLY | O_CREAT;
        flags |= command->append ? O_APPEND : O_TRUNC;
        int fd = open(command->output, flags, 0640);
        if (fd < 0) {
            perror(command->output);
            return -1;
        }
        if (redirect_fd(fd, STDOUT_FILENO) < 0)return -1;
    }
    return 0;
}

static int run_parent_builtin(Command *command){
    int saved_input = dup(STDIN_FILENO);
    int saved_output = dup(STDOUT_FILENO);
    if (saved_input < 0 || saved_output < 0) {
        perror("dup");
        if (saved_input >= 0) close(saved_input);
        if (saved_output >= 0) close(saved_output);
        return 1;
    }
    int result = apply_redirection(command) < 0 ? 1 : run_builtin(command);
    if (fflush(stdout) == EOF) {
        perror("stdout");
        result = 1;
    }
    if (redirect_fd(saved_input, STDIN_FILENO) < 0) result = 1;
    if (redirect_fd(saved_output, STDOUT_FILENO) < 0) result = 1;
    clearerr(stdout);
    return result;
}

static void close_pipes(int (*pipes)[2], size_t count){
    for (size_t i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static JobResult execute_job(Command *commands, size_t count, int interactive){
    JobResult result = {1, 0};
    fflush(NULL);
    if (count == 1 && is_builtin(commands[0].args[0])) {
        result.code = run_parent_builtin(commands);
        return result;
    }
    size_t pipe_count = count - 1;
    int (*pipes)[2] = NULL;
    if (pipe_count)
        pipes = checked_realloc(NULL, pipe_count * sizeof(*pipes));
    for (size_t i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            close_pipes(pipes, i);
            free(pipes);
            return result;
        }
    }
    pid_t *children = checked_realloc(NULL, count * sizeof(*children));
    size_t started = 0;
    int failed = 0;
    for (size_t i = 0; i < count; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            failed = 1;
            break;
        }
        if (pid == 0) {
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            } else if (!interactive && !commands[i].input) {
                int fd = open("/dev/null", O_RDONLY);
                if (fd < 0) {
                    perror("/dev/null");
                    _exit(1);
                }
                if (redirect_fd(fd, STDIN_FILENO) < 0)
                    _exit(1);
            }
            if (i < pipe_count &&
                dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                perror("dup2");
                _exit(1);
            }
            close_pipes(pipes, pipe_count);
            if (apply_redirection(&commands[i]) < 0) _exit(1);
            if (is_builtin(commands[i].args[0])) {
                int code = run_builtin(&commands[i]);
                if (fflush(stdout) == EOF) code = 1;
                _exit(code);
            }
            char *path = find_executable(commands[i].args[0]);
            if (!path) {
                fprintf(stderr, "mysh: %s: command not found\n", commands[i].args[0]);
                _exit(127);
            }
            execv(path, commands[i].args);
            perror(path);
            free(path);
            _exit(126);
        }
        children[started++] = pid;
    }
    close_pipes(pipes, pipe_count);
    free(pipes);
    //Wait for every stage, using the last stage's result for the job. 
    for (size_t i = 0; i < started; i++) {
        int status;
        pid_t waited;
        do {
            waited = waitpid(children[i], &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited < 0) {
            perror("waitpid");
            failed = 1;
        } else if (i == count - 1) {
            if (WIFEXITED(status)) {
                result.code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                result.signal = WTERMSIG(status);
            }
        }
    }
    free(children);
    if (failed) {
        result.code = 1;
        result.signal = 0;
    }
    return result;
}
int main(int argc, char **argv){
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [file]\n", argv[0]);
        return EXIT_SUCCESS;
    }
    int input = STDIN_FILENO;
    if (argc == 2) {
        input = open(argv[1], O_RDONLY);
        if (input < 0) {
            perror(argv[1]);
            return EXIT_FAILURE;
        }
        if (fcntl(input, F_SETFD, FD_CLOEXEC) < 0)
            perror("fcntl");
    }
    home_directory = getenv("HOME");
    int interactive = argc == 1 && isatty(STDIN_FILENO);
    if (interactive)
        puts("Welcome to my shell!");
    for (;;) {
        if (interactive) print_prompt();
        char *line = read_line(input);
        if (!line) break;
        size_t token_count;
        char **tokens = tokenize(line, &token_count);
        free(line);
        if (!token_count) {
            free(tokens);
            continue;
        }
        size_t bytes = (token_count + 1) * sizeof(Command);
        Command *commands = checked_realloc(NULL, bytes);
        memset(commands, 0, bytes);
        size_t command_count;
        JobResult result = {1, 0};
        int should_exit = 0;
        if (parse_commands(tokens, token_count, commands, &command_count) == 0) {
            for (size_t i = 0; i < command_count; i++) {
                if (!strcmp(commands[i].args[0], "exit")) should_exit = 1;
            }
            result = execute_job(commands, command_count, interactive);
        }
        for (size_t i = 0; i < token_count; i++)
            free(tokens[i]);
        free(tokens);
        free_commands(commands, command_count);
        if (interactive) {
            if (result.signal) printf("Terminated by signal %s\n", strsignal(result.signal));
            else if (result.code) printf("Exited with status %d\n", result.code);
        }
        if (should_exit) break;
    }
    if (input != STDIN_FILENO) close(input);
    if (interactive) puts("Exiting my shell.");
    return EXIT_SUCCESS;
}
