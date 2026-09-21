#include <stdio.h>
#include <unistd.h>

int main()
{
    int pipefd[2];

    pipe(pipefd);

    printf("Read end  = %d\n", pipefd[0]);
    printf("Write end = %d\n", pipefd[1]);

    return 0;
}
