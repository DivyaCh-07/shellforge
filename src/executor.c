#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "executor.h"
#include "builtin.h"
#include "job.h"
#include "jobcontroller.h"


/* ---------------------------------------------------------
   Terminal control
   --------------------------------------------------------- */

static void give_terminal_to(pid_t pgid)
{
    if (tcsetpgrp(STDIN_FILENO, pgid) == -1)
    {
        perror("tcsetpgrp");
    }
}


static void take_terminal_back(void)
{
    pid_t shell_pgid = getpgrp();

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1)
    {
        perror("tcsetpgrp");
    }
}


/* ---------------------------------------------------------
   Restore default signals in child
   --------------------------------------------------------- */

static void restore_child_signals(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}


/* ---------------------------------------------------------
   Build command string
   --------------------------------------------------------- */

static void build_command_string(command_t *cmd,
                                 char *buffer,
                                 size_t size)
{
    size_t i;
    size_t used = 0;

    buffer[0] = '\0';

    for (i = 0; i < (size_t)cmd->argc; i++)
    {
        size_t length;

        if (cmd->argv[i] == NULL)
            continue;

        length = strlen(cmd->argv[i]);

        if (i > 0)
        {
            if (used + 1 >= size)
                break;

            buffer[used++] = ' ';
            buffer[used] = '\0';
        }

        if (used + length >= size)
            break;

        strcpy(buffer + used, cmd->argv[i]);
        used += length;
    }
}


/* ---------------------------------------------------------
   Build pipeline string
   --------------------------------------------------------- */

static void build_pipeline_string(pipeline_t *pipeline,
                                   char *buffer,
                                   size_t size)
{
    size_t i;
    size_t used = 0;

    buffer[0] = '\0';

    for (i = 0; i < (size_t)pipeline->command_count; i++)
    {
        char command_text[MAX_JOB_COMMAND];
        size_t length;

        build_command_string(&pipeline->commands[i],
                              command_text,
                              sizeof(command_text));

        length = strlen(command_text);

        if (i > 0)
        {
            if (used + 3 >= size)
                break;

            strcpy(buffer + used, " | ");
            used += 3;
        }

        if (used + length >= size)
            break;

        strcpy(buffer + used, command_text);
        used += length;
    }
}


/* ---------------------------------------------------------
   Execute single command
   --------------------------------------------------------- */

int execute_command(command_t *cmd)
{
    pid_t pid;
    int status;
    int background;
    char command_text[MAX_JOB_COMMAND];

    if (cmd == NULL || cmd->argc == 0)
        return 0;

    /*
     * jobs, fg and bg must execute in the shell process.
     */
    if (is_job_builtin(cmd))
    {
        return execute_builtin(cmd);
    }

    background = cmd->background;

    build_command_string(cmd,
                         command_text,
                         sizeof(command_text));

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }


    /* -----------------------------------------------------
       Child
       ----------------------------------------------------- */

    if (pid == 0)
    {
        int fd;

        restore_child_signals();

        /*
         * Child becomes leader of its own process group.
         */
        if (setpgid(0, 0) == -1)
        {
            perror("setpgid");
            exit(EXIT_FAILURE);
        }

        /*
         * Background process gets /dev/null as stdin.
         */
        if (background)
        {
            fd = open("/dev/null", O_RDONLY);

            if (fd < 0)
            {
                perror("/dev/null");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        /*
         * Input redirection.
         */
        if (cmd->input[0] != '\0')
        {
            fd = open(cmd->input, O_RDONLY);

            if (fd < 0)
            {
                perror("open input");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        /*
         * Output redirection.
         */
        if (cmd->output[0] != '\0')
        {
            int flags = O_WRONLY | O_CREAT;

            if (cmd->append)
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;

            fd = open(cmd->output,
                      flags,
                      0644);

            if (fd < 0)
            {
                perror("open output");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        /*
         * Builtin.
         */
        if (is_builtin(cmd))
        {
            exit(execute_builtin(cmd));
        }

        /*
         * External command.
         */
        execvp(cmd->argv[0], cmd->argv);

        perror(cmd->argv[0]);
        exit(EXIT_FAILURE);
    }


    /* -----------------------------------------------------
       Parent
       ----------------------------------------------------- */

    /*
     * Parent also puts child into its own process group.
     */
    if (setpgid(pid, pid) == -1)
    {
        if (errno != EACCES && errno != ESRCH)
        {
            perror("setpgid");
        }
    }


    /* -----------------------------------------------------
       Background command
       ----------------------------------------------------- */

    if (background)
    {
        int job_id;

        job_id = job_add(pid,
                         command_text,
                         JOB_RUNNING);

        if (job_id > 0)
        {
            printf("[%d] Running %s\n",
                   job_id,
                   command_text);
        }

        return 0;
    }


    /* -----------------------------------------------------
       Foreground command
       ----------------------------------------------------- */

    give_terminal_to(pid);

    while (1)
    {
        pid_t result;

        result = waitpid(pid,
                         &status,
                         WUNTRACED);

        if (result == -1)
        {
            if (errno == EINTR)
                continue;

            perror("waitpid");
            take_terminal_back();
            return -1;
        }

        break;
    }


    /* -----------------------------------------------------
       Process stopped
       ----------------------------------------------------- */

    if (WIFSTOPPED(status))
    {
        int job_id;

        job_id = job_add(pid,
                         command_text,
                         JOB_STOPPED);

        if (job_id > 0)
        {
            printf("\n[%d] Stopped %s\n",
                   job_id,
                   command_text);
        }

        take_terminal_back();

        return 0;
    }


    /* -----------------------------------------------------
       Process exited
       ----------------------------------------------------- */

    if (WIFEXITED(status))
    {
        int result = WEXITSTATUS(status);

        take_terminal_back();

        return result;
    }


    /* -----------------------------------------------------
       Process killed by signal
       ----------------------------------------------------- */

    if (WIFSIGNALED(status))
    {
        take_terminal_back();

        if (WTERMSIG(status) != SIGINT &&
            WTERMSIG(status) != SIGQUIT)
        {
            fprintf(stderr,
                    "Process terminated by signal %d\n",
                    WTERMSIG(status));
        }

        return -1;
    }

    take_terminal_back();

    return 0;
}


/* ---------------------------------------------------------
   Execute pipeline
   --------------------------------------------------------- */

int execute_pipeline(pipeline_t *pipeline)
{

    int pipes[MAX_COMMANDS - 1][2];
    

    pid_t pgid = 0;

    int i;
    int status;
    int background;

    char command_text[MAX_JOB_COMMAND];

    if (pipeline == NULL ||
        pipeline->command_count == 0)
    {
        return 0;
    }
  /*
     * jobs, fg and bg must execute in the shell process.
     */
    if (pipeline->command_count == 1)
    {
        command_t *cmd = &pipeline->commands[0];

        if (is_job_builtin(cmd))
        {
            return execute_builtin(cmd);
        }
    }
    background =
        pipeline->commands[
            pipeline->command_count - 1
        ].background;

    build_pipeline_string(pipeline,
                           command_text,
                           sizeof(command_text));


    /* -----------------------------------------------------
       Create pipes
       ----------------------------------------------------- */

    for (i = 0;
         i < pipeline->command_count - 1;
         i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            return -1;
        }
    }


    /* -----------------------------------------------------
       Create processes
       ----------------------------------------------------- */

    for (i = 0;
         i < pipeline->command_count;
         i++)
    {
        pid_t pid;

        pid = fork();

        if (pid < 0)
        {
            perror("fork");
            return -1;
        }


        /* -------------------------------------------------
           Child
           ------------------------------------------------- */

        if (pid == 0)
        {
            int fd;
            int j;

            restore_child_signals();

            /*
             * First process creates process group.
             */
            if (i == 0)
            {
                if (setpgid(0, 0) == -1)
                {
                    perror("setpgid");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (setpgid(0, pgid) == -1)
                {
                    if (errno != EACCES &&
                        errno != ESRCH)
                    {
                        perror("setpgid");
                    }
                }
            }


            /* Input from previous pipe. */
            if (i > 0)
            {
                if (dup2(pipes[i - 1][0],
                         STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }


            /* Output to next pipe. */
            if (i < pipeline->command_count - 1)
            {
                if (dup2(pipes[i][1],
                         STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }


            /*
             * Background pipeline:
             * first command reads from /dev/null.
             */
            if (background && i == 0)
            {
                fd = open("/dev/null", O_RDONLY);

                if (fd < 0)
                {
                    perror("/dev/null");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDIN_FILENO);
                close(fd);
            }


            /* Input redirection. */
            if (pipeline->commands[i].input[0] != '\0')
            {
                fd = open(
                    pipeline->commands[i].input,
                    O_RDONLY);

                if (fd < 0)
                {
                    perror("open input");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDIN_FILENO);
                close(fd);
            }


            /* Output redirection. */
            if (pipeline->commands[i].output[0] != '\0')
            {
                int flags = O_WRONLY | O_CREAT;

                if (pipeline->commands[i].append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;

                fd = open(
                    pipeline->commands[i].output,
                    flags,
                    0644);

                if (fd < 0)
                {
                    perror("open output");
                    exit(EXIT_FAILURE);
                }

                dup2(fd, STDOUT_FILENO);
                close(fd);
            }


            /* Close all pipe descriptors. */
            for (j = 0;
                 j < pipeline->command_count - 1;
                 j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }


            /* Builtin inside pipeline. */
            if (is_builtin(
                    &pipeline->commands[i]))
            {
                exit(execute_builtin(
                    &pipeline->commands[i]));
            }


            /* External command. */
            execvp(
                pipeline->commands[i].argv[0],
                pipeline->commands[i].argv);

            perror(
                pipeline->commands[i].argv[0]);

            exit(EXIT_FAILURE);
        }


        /* -------------------------------------------------
           Parent
           ------------------------------------------------- */

        

        if (i == 0)
        {
            pgid = pid;
        }

        if (setpgid(pid, pgid) == -1)
        {
            if (errno != EACCES &&
                errno != ESRCH)
            {
                perror("setpgid");
            }
        }
    }


    /* -----------------------------------------------------
       Parent closes all pipes
       ----------------------------------------------------- */

    for (i = 0;
         i < pipeline->command_count - 1;
         i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }


    /* -----------------------------------------------------
       Background pipeline
       ----------------------------------------------------- */

    if (background)
    {
        int job_id;

        job_id = job_add(
            pgid,
            command_text,
            JOB_RUNNING);

        if (job_id > 0)
        {
            printf("[%d] Running %s\n",
                   job_id,
                   command_text);
        }

        return 0;
    }


    /* -----------------------------------------------------
       Foreground pipeline
       ----------------------------------------------------- */

    give_terminal_to(pgid);

    while (1)
    {
        pid_t result;

        result = waitpid(-pgid,
                         &status,
                         WUNTRACED);

        if (result == -1)
        {
            if (errno == EINTR)
                continue;

            if (errno == ECHILD)
                break;

            perror("waitpid");
            break;
        }

        /*
         * If one process in the pipeline is stopped,
         * the whole pipeline becomes a stopped job.
         */
        if (WIFSTOPPED(status))
        {
            int job_id;

            /*
             * Stop the complete process group.
             */
            kill(-pgid, SIGSTOP);

            job_id = job_add(
                pgid,
                command_text,
                JOB_STOPPED);

            if (job_id > 0)
            {
                printf("\n[%d] Stopped %s\n",
                       job_id,
                       command_text);
            }

            take_terminal_back();

            return 0;
        }

        /*
         * Continue waiting until all children are gone.
         */
        if (WIFEXITED(status) ||
            WIFSIGNALED(status))
        {
            continue;
        }
    }

    take_terminal_back();

    return 0;
}
