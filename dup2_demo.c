#include <stdio.h>
#include <unistd.h>

int main()
{
    int pipefd[2];

    pipe(pipefd);

    printf("Before dup2\n");

    dup2(pipefd[1], STDOUT_FILENO);

    printf("This goes into the pipe\n");

    return 0;
}
