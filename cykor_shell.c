#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100

// 프롬프트 출력: 현재 디렉토리 basename + "$ "
void print_prompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s$ ", basename(cwd));
    } else {
        perror("getcwd");
        printf("$ ");
    }
    fflush(stdout);
}

// 한 줄 명령어 실행: cd, pwd 직접 처리, 나머지는 execvp
void execute_command(char *line) {
    // 토큰화
    char *argv[MAX_ARGS];
    int argc = 0;
    char *token = strtok(line, " \t");
    while (token && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;

    if (argc == 0)
        return;

    // cd
    if (strcmp(argv[0], "cd") == 0) {
        char *dir = (argc > 1) ? argv[1] : getenv("HOME");
        if (chdir(dir) < 0)
            perror("cd");
        return;
    }

    // pwd
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)))
            printf("%s\n", cwd);
        else
            perror("pwd");
        return;
    }
}

int main(void) {
    char *line = NULL;
    size_t len = 0;

    while (1) {
        print_prompt();
        if (getline(&line, &len, stdin) == -1)
            break;  // EOF
        
        // 개행 제거
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n')
            line[l-1] = '\0';

        if (strcmp(line, "exit") == 0)
            break;

        execute_command(line);
    }

    free(line);
    return 0;
}
