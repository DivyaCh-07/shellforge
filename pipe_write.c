#include <stdio.h>
#include <unistd.h>

int main()
{
    int pipefd[2];
    char buffer[100];

    pipe(pipefd);

    write(pipefd[1], "Hello from pipe!\n", 17);

    int n = read(pipefd[0], buffer, sizeof(buffer) - 1);

    buffer[n] = '\0';

    printf("Received: %s", buffer);

    return 0;
}
