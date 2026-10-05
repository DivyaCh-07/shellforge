#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "job.h"
#include "jobcontroller.h"


/* -------------------- cd -------------------- */

static int builtin_cd(command_t *cmd)
{
    const char *directory;

    if (cmd->argc == 1)
    {
        directory = getenv("HOME");

        if (directory == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return -1;
        }
    }
    else if (cmd->argc == 2)
    {
        directory = cmd->argv[1];
    }
    else
    {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
    }

    if (chdir(directory) != 0)
    {
        perror("cd");
        return -1;
    }

    return 0;
}


/* -------------------- pwd -------------------- */

static int builtin_pwd(command_t *cmd)
{
    char current_directory[4096];

    if (cmd->argc > 1)
    {
        fprintf(stderr, "pwd: too many arguments\n");
        return -1;
    }

    if (getcwd(current_directory,
               sizeof(current_directory)) == NULL)
    {
        perror("pwd");
        return -1;
    }

    printf("%s\n", current_directory);

    return 0;
}


/* -------------------- echo -------------------- */

static int builtin_echo(command_t *cmd)
{
    int i;

    for (i = 1; i < cmd->argc; i++)
    {
        printf("%s", cmd->argv[i]);

        if (i < cmd->argc - 1)
            printf(" ");
    }

    printf("\n");

    return 0;
}


/* -------------------- exit -------------------- */

static int builtin_exit(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "exit: too many arguments\n");
        return -1;
    }

    return 1;
}


/* -------------------- jobs -------------------- */

static int builtin_jobs(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "jobs: too many arguments\n");
        return -1;
    }

    jobs_print();

    return 0;
}


/* -------------------- Get job ID -------------------- */

static int get_job_id(command_t *cmd)
{
    const char *value;
    char *endptr;
    long job_id;

    if (cmd->argc != 2)
    {
        fprintf(stderr,
                "%s: usage: %s %%job_id\n",
                cmd->argv[0],
                cmd->argv[0]);

        return -1;
    }

    value = cmd->argv[1];

    if (value[0] == '%')
        value++;

    job_id = strtol(value, &endptr, 10);

    if (*value == '\0' || *endptr != '\0')
    {
        fprintf(stderr,
                "%s: invalid job id\n",
                cmd->argv[0]);

        return -1;
    }

    return (int)job_id;
}


/* -------------------- bg -------------------- */

static int builtin_bg(command_t *cmd)
{
    int job_id;
    job_t *job;

    job_id = get_job_id(cmd);

    if (job_id < 0)
        return -1;

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(stderr,
                "bg: job %d not found\n",
                job_id);

        return -1;
    }

    if (job->state == JOB_DONE)
    {
        fprintf(stderr,
                "bg: job %d is already done\n",
                job_id);

        return -1;
    }

    jobcontroller_continue(job->pgid);

    printf("[%d] Running %s\n",
           job->job_id,
           job->command);

    return 0;
}


/* -------------------- fg -------------------- */

static int builtin_fg(command_t *cmd)
{
    int job_id;
    job_t *job;
    pid_t pgid;

    job_id = get_job_id(cmd);

    if (job_id < 0)
        return -1;

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(stderr,
                "fg: job %d not found\n",
                job_id);

        return -1;
    }

    if (job->state == JOB_DONE)
    {
        fprintf(stderr,
                "fg: job %d is already done\n",
                job_id);

        return -1;
    }

    pgid = job->pgid;

    printf("%s\n", job->command);

    /*
     * Continue stopped job.
     */
    if (job->state == JOB_STOPPED)
    {
        jobcontroller_continue(pgid);
    }

    /*
     * Give terminal to job and wait.
     */
    jobcontroller_wait_foreground(pgid);

    return 0;
}


/* -------------------- Check builtin -------------------- */

int is_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return 0;

    if (strcmp(cmd->argv[0], "cd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "pwd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "echo") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "exit") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "jobs") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "fg") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "bg") == 0)
        return 1;

    return 0;
}


/* -------------------- Job builtin check -------------------- */

int is_job_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return 0;

    if (strcmp(cmd->argv[0], "jobs") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "fg") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "bg") == 0)
        return 1;

    return 0;
}


/* -------------------- Execute builtin -------------------- */

int execute_builtin(command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return -1;

    if (strcmp(cmd->argv[0], "cd") == 0)
        return builtin_cd(cmd);

    if (strcmp(cmd->argv[0], "pwd") == 0)
        return builtin_pwd(cmd);

    if (strcmp(cmd->argv[0], "echo") == 0)
        return builtin_echo(cmd);

    if (strcmp(cmd->argv[0], "exit") == 0)
        return builtin_exit(cmd);

    if (strcmp(cmd->argv[0], "jobs") == 0)
        return builtin_jobs(cmd);

    if (strcmp(cmd->argv[0], "fg") == 0)
        return builtin_fg(cmd);

    if (strcmp(cmd->argv[0], "bg") == 0)
        return builtin_bg(cmd);

    return -1;
}
