#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>
#include <signal.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;
int history_index = 0;

void print_prompt() {
    char cwd[PATH_MAX];
    char *username = getenv("USER");
    char hostname[256];
    
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "unknown");
    }
    
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    
    if (username == NULL) {
        struct passwd *pw = getpwuid(getuid());
        username = pw ? pw->pw_name : "user";
    }
    
    printf("\033[1;32m%s@%s\033[0m:\033[1;34m%s\033[0m$ ", username, hostname, cwd);
    fflush(stdout);
}

void add_to_history(char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;
    
    // Избегаем дублирования последней команды
    if (history_count > 0 && strcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0) {
        return;
    }
    
    if (history[history_count % HISTORY_SIZE] != NULL) {
        free(history[history_count % HISTORY_SIZE]);
    }
    
    history[history_count % HISTORY_SIZE] = strdup(cmd);
    history_count++;
    history_index = history_count;
}

void show_history() {
    int start = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE : 0;
    int end = history_count;
    
    for (int i = start; i < end; i++) {
        if (history[i % HISTORY_SIZE] != NULL) {
            printf("%4d  %s\n", i + 1, history[i % HISTORY_SIZE]);
        }
    }
}

char **parse_command(char *cmd) {
    char **args = malloc(MAX_ARGS * sizeof(char*));
    char *token;
    int i = 0;
    
    token = strtok(cmd, " \t\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i] = token;
        i++;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    
    return args;
}

int builtin_cd(char **args) {
    char *dir;
    
    if (args[1] == NULL) {
        dir = getenv("HOME");
        if (dir == NULL) {
            struct passwd *pw = getpwuid(getuid());
            dir = pw ? pw->pw_dir : "/";
        }
    } else {
        dir = args[1];
    }
    
    if (chdir(dir) != 0) {
        perror("dermosh: cd");
        return 1;
    }
    return 0;
}

int builtin_pwd(char **args __attribute__((unused))) {
    char cwd[PATH_MAX];
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("dermosh: pwd");
        return 1;
    }
    return 0;
}

int builtin_echo(char **args) {
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i + 1] != NULL) printf(" ");
    }
    printf("\n");
    return 0;
}

int builtin_exit(char **args __attribute__((unused))) {
    printf("Goodbye from dermosh!\n");
    
    // Освобождаем память истории
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (history[i] != NULL) {
            free(history[i]);
        }
    }
    
    exit(0);
}

int builtin_help(char **args __attribute__((unused))) {
    printf("Dermosh - простая терминальная оболочка\n");
    printf("Встроенные команды:\n");
    printf("  cd [dir]     - изменить директорию\n");
    printf("  pwd          - показать текущую директорию\n");
    printf("  echo [args]  - вывести аргументы\n");
    printf("  history      - показать историю команд\n");
    printf("  help         - показать эту справку\n");
    printf("  exit         - выйти из оболочки\n");
    printf("\nИспользуйте Tab для автодополнения файлов\n");
    printf("Ctrl+C для прерывания, Ctrl+D для выхода\n");
    return 0;
}

int execute_builtin(char **args) {
    if (args[0] == NULL) return 0;
    
    if (strcmp(args[0], "cd") == 0) {
        return builtin_cd(args);
    }
    if (strcmp(args[0], "pwd") == 0) {
        return builtin_pwd(args);
    }
    if (strcmp(args[0], "echo") == 0) {
        return builtin_echo(args);
    }
    if (strcmp(args[0], "exit") == 0) {
        return builtin_exit(args);
    }
    if (strcmp(args[0], "history") == 0) {
        show_history();
        return 0;
    }
    if (strcmp(args[0], "help") == 0) {
        return builtin_help(args);
    }
    
    return -1; // не встроенная команда
}

int execute_external(char **args) {
    pid_t pid;
    int status;
    
    pid = fork();
    
    if (pid == 0) {
        // Дочерний процесс
        if (execvp(args[0], args) == -1) {
            printf("dermosh: команда '%s' не найдена\n", args[0]);
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("dermosh: fork");
        return 1;
    } else {
        // Родительский процесс
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    
    return status;
}

int execute_command(char *cmd) {
    char **args = parse_command(cmd);
    int result;
    
    if (args[0] == NULL) {
        free(args);
        return 0;
    }
    
    result = execute_builtin(args);
    if (result == -1) {
        result = execute_external(args);
    }
    
    free(args);
    return result;
}

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\n");
        print_prompt();
        fflush(stdout);
    }
}

int main() {
    char input[MAX_CMD_LEN];
    
    // Устанавливаем обработчик сигналов
    signal(SIGINT, handle_signal);
    
    printf("Добро пожаловать в Dermosh v1.0!\n");
    printf("Введите 'help' для справки или 'exit' для выхода.\n\n");
    
    while (1) {
        print_prompt();
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // Удаляем символ новой строки
        input[strcspn(input, "\n")] = 0;
        
        // Пропускаем пустые строки
        if (strlen(input) == 0) {
            continue;
        }
        
        add_to_history(input);
        execute_command(input);
    }
    
    printf("Goodbye from dermosh!\n");
    
    // Освобождаем память истории
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (history[i] != NULL) {
            free(history[i]);
        }
    }
    
    return 0;
}