#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100
#define MAX_CMDS 10

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

// 한 줄 명령어 실행: cd, pwd 직접 처리, 나머지는 execvp(나중에)
void execute_command(char *line) {
    char *cmds[MAX_CMDS];
    int ncmd = 0;
    char *token = strtok(line, "|");
    while (token && ncmd < MAX_CMDS) {
        cmds[ncmd++] = token;
        token = strtok(NULL, "|");
    }
    if (ncmd == 0) return;
    // 토큰화
    if (ncmd == 1){
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
    int pipes[MAX_CMDS - 1][2];
    pid_t pids[MAX_CMDS];
    for (int i = 0; i < ncmd; i++) {
        // 각 서브 명령
        char *argv[MAX_ARGS];
        int argc = 0;
        char *arg = strtok(cmds[i], " \t");
        while (arg && argc < MAX_ARGS - 1) {
            argv[argc++] = arg;
            arg = strtok(NULL, " \t");
        }
        argv[argc] = NULL;
        if (argc == 0) continue;

        pids[i] = fork();
        if (pids[i] == 0) {
            for (int j = 0; j < ncmd - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            execvp(argv[0], argv);
            perror(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    // 부모
    for (int i = 0; i < ncmd - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // 자식 대기
    for (int i = 0; i < ncmd; i++) {
        waitpid(pids[i], NULL, 0);
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
