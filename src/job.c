#include <stdio.h>
#include <string.h>

#include "job.h"

static job_t job_table[MAX_JOBS];
static int next_job_id = 1;

void jobs_init(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        job_table[i].job_id = 0;
        job_table[i].pgid = 0;
        job_table[i].state = JOB_DONE;
        job_table[i].command[0] = '\0';
    }

    next_job_id = 1;
}

int job_add(pid_t pgid, const char *command, job_state_t state)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id == 0)
        {
            job_table[i].job_id = next_job_id++;
            job_table[i].pgid = pgid;
            job_table[i].state = state;

            strncpy(job_table[i].command,
                    command,
                    MAX_JOB_COMMAND - 1);

            job_table[i].command[MAX_JOB_COMMAND - 1] = '\0';

            return job_table[i].job_id;
        }
    }

    return -1;
}

job_t *job_find(int job_id)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id == job_id)
            return &job_table[i];
    }

    return NULL;
}

job_t *job_find_by_pgid(pid_t pgid)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id != 0 &&
            job_table[i].pgid == pgid)
        {
            return &job_table[i];
        }
    }

    return NULL;
}

void job_remove(int job_id)
{
    job_t *job = job_find(job_id);

    if (job != NULL)
    {
        job->job_id = 0;
        job->pgid = 0;
        job->state = JOB_DONE;
        job->command[0] = '\0';
    }
}

void jobs_print(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
    {
        if (job_table[i].job_id != 0)
        {
            printf("[%d] ", job_table[i].job_id);

            if (job_table[i].state == JOB_RUNNING)
            {
                printf("Running");
            }
            else if (job_table[i].state == JOB_STOPPED)
            {
                printf("Stopped");
            }
            else
            {
                printf("Done");
            }

            printf(" %s\n", job_table[i].command);
        }
    }
}

void job_stop(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job != NULL)
        job->state = JOB_STOPPED;
}

void job_continue(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job != NULL)
        job->state = JOB_RUNNING;
}

void job_done(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job != NULL)
        job->state = JOB_DONE;
}
