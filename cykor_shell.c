#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <pwd.h>
#include <stdbool.h>
#include <ctype.h> 
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100
#define MAX_CMDS 10

void print_prompt();
int execute_command(char *line);
int execute_line(char *line);
void reap_children(int signum);

// 프롬프트 출력
void print_prompt() {
    char user[30];
    char host[50];
    char cwd[PATH_MAX];
    char buf[PATH_MAX + 80 + 100];
    
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strncpy(user, pw->pw_name, sizeof(user));
    } else {
        strcpy(user, "unknown");
    }
    if (gethostname(host, sizeof(host)) < 0) {
        strcpy(host, "unknown");
    }

    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "unknown");
    }

    char super = (getuid() == 0) ? '#' : '$';
    snprintf(buf, sizeof(buf),"[\001\e[1;32m\002%s@%s\001\e[0m\002:~\001\e[1;35m\002%s\001\e[0m\002]\001\e[0m\002%c ", user, host, cwd, super);

    fputs(buf, stdout);
    fflush(stdout);
}


// 한 줄 명령어 실행: cd, pwd 직접 처리, 나머지는 execvp
int execute_command(char *line) {
    char *cmds[MAX_CMDS];
    int ncmd = 0;
    char *save1;
    char *token1 = strtok_r(line, "|", &save1);
    while (token1 && ncmd < MAX_CMDS) {
        cmds[ncmd++] = token1;
        token1 = strtok_r(NULL, "|", &save1);
    }
    if (ncmd == 0) {
        return 0;
    }
    // 토큰화
    if (ncmd == 1){
        char *argv[MAX_ARGS];
        int argc = 0;
        char *save2;
        char *token2 = strtok_r(line, " \t", &save2);
        while (token2 && argc < MAX_ARGS - 1) {
            argv[argc++] = token2;
            token2 = strtok_r(NULL, " \t", &save2);
        }
        argv[argc] = NULL;

        if (argc == 0) {
            return 0;
        }

        // cd
        if (strcmp(argv[0], "cd") == 0) {
            char *dir;
            if (argc > 1 && strcmp(argv[1], "~") != 0) {
                dir = argv[1];
            } else {
                dir = getenv("HOME");
            }
            if (chdir(dir) < 0) {
                perror("cd");
                return 1;
            }
            return 0;
        }

        // pwd
        if (strcmp(argv[0], "pwd") == 0) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd))){
                printf("%s\n", cwd);
                return 0;
            } else {
                perror("pwd");
                return 1;
            }
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {
            execvp(argv[0], argv);
            perror(argv[0]);
            exit(EXIT_FAILURE);
        } else {
            int st;
            waitpid(pid, &st, 0);
            return WEXITSTATUS(st);
        }
    }
    int pipes[MAX_CMDS - 1][2];
    for (int i = 0; i < ncmd - 1; i++) {
        if (pipe(pipes[i]) < 0){
            perror("pipe");
            return 1;
        }
    }
    pid_t pids[MAX_CMDS];
    for (int i = 0; i < ncmd; i++) {
        // 서브 cmd
        char *argv[MAX_ARGS];
        int argc = 0;
        char *save3;
        char *arg = strtok_r(cmds[i], " \t", &save3);
        while (arg && argc < MAX_ARGS - 1) {
            argv[argc++] = arg;
            arg = strtok_r(NULL, " \t", &save3);
        }
        argv[argc] = NULL;
        if (argc == 0) continue;

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return 1;
        }
        if (pids[i] == 0) {
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < ncmd - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
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
    int status = 0, st;
    for (int i = 0; i < ncmd; i++) {
        waitpid(pids[i], &st, 0);
        status = WEXITSTATUS(st);
    }
    return status;
}

int execute_line(char *line) {
    char *ptr = line;
    char sep[3] = "";   // 이전 구분자 저장
    int status = 0;

    while (*ptr) {
        // 다음 위치 찾기
        char *next = NULL;
        char found[3] = "";
        int len = 0;
        for (char *p = ptr; *p; ++p) {
            if (p[0]=='&' && p[1]=='&') {
                next = p;
                strcpy(found,"&&");
                len=2;
                break;
            }
            if (p[0]=='|' && p[1]=='|') {
                next = p;
                strcpy(found,"||");
                len=2;
                break;
            }
            if (*p==';') {
                next = p;
                strcpy(found,";");
                len=1;
                break;
            }
        }
        // 분리
        if (next) *next = '\0';

        // 조건에 따라 건너뛰기
        if (!sep[0] || strcmp(sep,";")==0 || (strcmp(sep,"&&")==0 && status==0) || (strcmp(sep,"||")==0 && status!=0)) {
            status = execute_command(ptr);
        }
        // 다음으로
        if (!next) break;
        strcpy(sep, found);
        ptr = next + len;
    }
    return status;
}

void reap_children(int signum) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("child %d terminated\n", pid);
    }
}


int main(void) {
    char *line = NULL;
    size_t len = 0;
    signal(SIGCHLD, reap_children);

    while (1) {
        print_prompt();
        if (getline(&line, &len, stdin) == -1)
            break;  // EOF
        
        // 개행 제거
        size_t l = strlen(line);
        if (l > 0 && line[l-1] == '\n')
            line[--l] = '\0';

        if (strcmp(line, "exit") == 0)
            break;

        bool background = false;
        while (l > 0 && isspace((unsigned char)line[l-1])) {
            line[--l] = '\0';
        }
        if (l > 0 && line[l-1] == '&' && !(l > 1 && line[l-2] == '&')) {
            background = true;
            line[--l] = '\0';
            while (l > 0 && isspace((unsigned char)line[l-1])) {
                line[--l] = '\0';
            }
        }

        if (background) {
            pid_t pid = fork();
            if (pid == 0) {
                execute_line(line);
                exit(0);
            } else if (pid > 0) {
                printf("[%d]\n", pid);
            } else {
                perror("fork");
            }
        } else {
            execute_line(line);
        }
    }
    free(line);
    return 0;
}