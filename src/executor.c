#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "executor.h"
#include "builtin.h"


/* Reap completed background processes */
static void reap_background_processes(int sig)
{
    int saved_errno = errno;
    int status;

    (void)sig;

    while (waitpid(-1, &status, WNOHANG) > 0)
    {
        /* Child has been collected */
    }

    errno = saved_errno;
}


/* Install SIGCHLD handler */
static void setup_sigchld(void)
{
    struct sigaction sa;

    sa.sa_handler = reap_background_processes;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    sigaction(SIGCHLD, &sa, NULL);
}


int execute_command(command_t *cmd)
{
    pid_t pid;
    int status;

    if (cmd == NULL || cmd->argc == 0)
        return -1;

    setup_sigchld();

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    /* Child */
    if (pid == 0)
    {
        char *args[MAX_ARGS + 1];
        int i;
        int fd;

        for (i = 0; i < cmd->argc; i++)
            args[i] = cmd->argv[i];

        args[cmd->argc] = NULL;

        /* Background commands should not read from terminal */
        if (cmd->background && cmd->input[0] == '\0')
        {
            fd = open("/dev/null", O_RDONLY);

            if (fd >= 0)
            {
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
        }

        /* Input redirection */
        if (cmd->input[0] != '\0')
        {
            fd = open(cmd->input, O_RDONLY);

            if (fd < 0)
            {
                perror("input");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        /* Output redirection */
        if (cmd->output[0] != '\0')
        {
            if (cmd->append)
            {
                fd = open(cmd->output,
                          O_WRONLY | O_CREAT | O_APPEND,
                          0644);
            }
            else
            {
                fd = open(cmd->output,
                          O_WRONLY | O_CREAT | O_TRUNC,
                          0644);
            }

            if (fd < 0)
            {
                perror("output");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        /* Builtin */
        if (is_builtin(cmd))
        {
            int result = execute_builtin(cmd);
            exit(result);
        }

        execvp(args[0], args);

        perror("Shellforge");
        exit(EXIT_FAILURE);
    }

    /* Background command */
    if (cmd->background)
    {
        printf("[Background PID: %d]\n", pid);
        return 0;
    }

    /* Foreground command */
    if (waitpid(pid, &status, 0) == -1)
    {
        if (errno == EINTR)
            return 0;

        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
    {
        fprintf(stderr,
                "Process terminated by signal %d\n",
                WTERMSIG(status));

        return -1;
    }

    return 0;
}


int execute_pipeline(pipeline_t *pipeline)
{
    int pipes[MAX_COMMANDS - 1][2];
    pid_t pids[MAX_COMMANDS];

    int i;
    int j;
    int status;

    if (pipeline == NULL || pipeline->command_count == 0)
        return -1;

    setup_sigchld();

    /* Create pipes */
    for (i = 0; i < pipeline->command_count - 1; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            return -1;
        }
    }

    /* Create children */
    for (i = 0; i < pipeline->command_count; i++)
    {
        pids[i] = fork();

        if (pids[i] < 0)
        {
            perror("fork");
            return -1;
        }

        /* Child */
        if (pids[i] == 0)
        {
            char *args[MAX_ARGS + 1];

            for (j = 0;
                 j < pipeline->commands[i].argc;
                 j++)
            {
                args[j] = pipeline->commands[i].argv[j];
            }

            args[pipeline->commands[i].argc] = NULL;

            /* Read from previous command */
            if (i > 0)
            {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            else if (pipeline->commands[i].background &&
                     pipeline->commands[i].input[0] == '\0')
            {
                int fd = open("/dev/null", O_RDONLY);

                if (fd >= 0)
                {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }

            /* Write to next command */
            if (i < pipeline->command_count - 1)
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            /* Output redirection on final command */
            if (i == pipeline->command_count - 1 &&
                pipeline->commands[i].output[0] != '\0')
            {
                int fd;

                if (pipeline->commands[i].append)
                {
                    fd = open(pipeline->commands[i].output,
                              O_WRONLY | O_CREAT | O_APPEND,
                              0644);
                }
                else
                {
                    fd = open(pipeline->commands[i].output,
                              O_WRONLY | O_CREAT | O_TRUNC,
                              0644);
                }

                if (fd < 0)
                {
                    perror("output");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            /* Close all pipe descriptors */
            for (j = 0;
                 j < pipeline->command_count - 1;
                 j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(args[0], args);

            perror("Shellforge");
            exit(EXIT_FAILURE);
        }
    }

    /* Parent closes all pipes */
    for (i = 0; i < pipeline->command_count - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    /*
     * If the pipeline is running in background,
     * do not wait for the children.
     */
    if (pipeline->commands[
            pipeline->command_count - 1].background)
    {
        printf("[Background pipeline started: %d]\n",
               pids[0]);

        return 0;
    }

    /* Foreground pipeline */
    for (i = 0; i < pipeline->command_count; i++)
    {
        waitpid(pids[i], &status, 0);
    }

    return 0;
}
