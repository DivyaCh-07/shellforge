#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "job.h"
#include "jobcontroller.h"

static pid_t shell_pgid = 0;

/*
 * Initialize job control.
 */
void jobcontroller_init(void)
{
    shell_pgid = getpgrp();

    jobs_init();
}

/*
 * Put a process into a process group.
 */
void jobcontroller_set_process_group(pid_t pid, pid_t pgid)
{
    if (setpgid(pid, pgid) == -1)
    {
        if (errno != EACCES && errno != ESRCH)
        {
            perror("setpgid");
        }
    }
}

/*
 * Stop an entire job/process group.
 */
void jobcontroller_stop(pid_t pgid)
{
    if (pgid <= 0)
        return;

    if (kill(-pgid, SIGSTOP) == -1)
    {
        perror("SIGSTOP");
        return;
    }

    job_stop(pgid);
}

/*
 * Continue an entire stopped job.
 */
void jobcontroller_continue(pid_t pgid)
{
    if (pgid <= 0)
        return;

    if (kill(-pgid, SIGCONT) == -1)
    {
        perror("SIGCONT");
        return;
    }

    job_continue(pgid);
}

/*
 * Give terminal to a job.
 */
static void give_terminal_to(pid_t pgid)
{
    if (pgid > 0)
    {
        if (tcsetpgrp(STDIN_FILENO, pgid) == -1)
        {
            perror("tcsetpgrp");
        }
    }
}

/*
 * Give terminal back to shell.
 */
static void give_terminal_back(void)
{
    if (shell_pgid > 0)
    {
        if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1)
        {
            perror("tcsetpgrp");
        }
    }
}

/*
 * Wait for all processes belonging to a foreground job.
 *
 * If any process is stopped, the complete job becomes STOPPED.
 */
void jobcontroller_wait_foreground(pid_t pgid)
{
    int status;
    int stopped = 0;
    int finished = 0;

    if (pgid <= 0)
        return;

    give_terminal_to(pgid);

    while (1)
    {
        pid_t pid;

        pid = waitpid(-pgid, &status, WUNTRACED);

        if (pid == -1)
        {
            if (errno == EINTR)
                continue;

            if (errno == ECHILD)
                break;

            perror("waitpid");
            break;
        }

        if (WIFSTOPPED(status))
        {
            stopped = 1;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            finished++;

            /*
             * Keep waiting until no children remain.
             */
            continue;
        }
    }

    give_terminal_back();

    if (stopped)
    {
        job_stop(pgid);
    }
    else
    {
        job_done(pgid);
    }

    (void)finished;
}
