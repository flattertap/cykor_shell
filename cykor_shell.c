#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>

void print_prompt() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s$ ", basename(cwd));
    } else {
        perror("getcwd");
    }
    fflush(stdout);
}

int main(void) {
    char *line = NULL;
    size_t len = 0;

    while (1) {
        print_prompt();
        if (getline(&line, &len, stdin) == -1) {
            printf("\n");
            break;
        }

        if (line[strlen(line)-1] == '\n')
            line[strlen(line)-1] = '\0';

        if (strlen(line) == 0)
            continue;

        printf("%s\n", line);
    }

    free(line);
    return 0;
}