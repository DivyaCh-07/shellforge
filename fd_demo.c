#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    int pipefd[2];

    // Create pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    // pipefd[0] = read end
    // pipefd[1] = write end
    fprintf(stderr, "pipefd[0] = %d (Read End)\n", pipefd[0]);
    fprintf(stderr, "pipefd[1] = %d (Write End)\n", pipefd[1]);

    fprintf(stderr, "\nBefore dup2(4,1):\n");
    fprintf(stderr, "FD 0 -> stdin/keyboard\n");
    fprintf(stderr, "FD 1 -> stdout/terminal\n");
    fprintf(stderr, "FD 2 -> stderr/terminal\n");
    fprintf(stderr, "FD %d -> pipe read end\n", pipefd[0]);
    fprintf(stderr, "FD %d -> pipe write end\n", pipefd[1]);

    // Make stdout (FD 1) point to pipe write end
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
        perror("dup2");
        return 1;
    }

    fprintf(stderr, "\nAfter dup2(%d,1):\n", pipefd[1]);
    fprintf(stderr, "FD 1 -> pipe write end\n");
    fprintf(stderr, "FD %d -> pipe write end\n", pipefd[1]);

    // Close original pipe write descriptor
    close(pipefd[1]);

    fprintf(stderr, "\nAfter close(%d):\n", pipefd[1]);
    fprintf(stderr, "FD 1 still points to pipe write end\n");

    // This goes into the pipe, NOT directly to terminal
    printf("Hello! This message goes through the pipe.\n");

    fflush(stdout);

    // Read the message from pipe
    char buffer[100];
    int n = read(pipefd[0], buffer, sizeof(buffer) - 1);

    if (n > 0) {
        buffer[n] = '\0';

        fprintf(stderr, "\nData received from pipe:\n");
        fprintf(stderr, "%s", buffer);
    }

    close(pipefd[0]);

    return 0;
}
