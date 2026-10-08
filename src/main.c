#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"
#include "jobcontroller.h"


/* ---------------------------------------------------------
   Initialize shell job control
   --------------------------------------------------------- */

static void init_job_control(void)
{
    pid_t shell_pgid;

    shell_pgid = getpid();

    /*
     * Put shell into its own process group.
     */
    if (setpgid(shell_pgid, shell_pgid) == -1 &&
        errno != EACCES)
    {
        perror("setpgid");
        exit(EXIT_FAILURE);
    }

    /*
     * Shell owns the terminal.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1)
    {
        perror("tcsetpgrp");
        exit(EXIT_FAILURE);
    }

    /*
     * Shell should not be stopped by terminal job-control
     * signals.
     */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}


/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main(void)
{
    char *line;

    init_job_control();
    jobcontroller_init();

    printf("=====================================\n");
    printf("Shellforge\n");
    printf(" A Unix Style Shell written in C\n");
    printf("=====================================\n");

    while (1)
    {
        token_list_t tokens;
        pipeline_t pipeline;

        line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }

        /*
         * Store command for UP arrow/history.
         */
        add_history(line);

        /*
         * Exit.
         */
        if (strcmp(line, "exit") == 0)
        {
            free(line);

            printf("Exiting...\n");

            break;
        }

        /*
         * Lexer.
         */
        if (!lexer(line, &tokens))
        {
            printf("Lexer failed.\n");

            free(line);

            continue;
        }

        /*
         * Display tokens.
         */
        token_print(&tokens);

        /*
         * Parser.
         */
        if (!parse(&tokens, &pipeline))
        {
            free(line);

            continue;
        }

        /*
         * Variable expansion.
         */
        expand_variables(&pipeline);

        /*
         * Display parsed pipeline.
         */
        pipeline_print(&pipeline);

        /*
         * Execute pipeline.
         */
        {
            int result;

            result = execute_pipeline(&pipeline);

            if (result == 1)
            {
                pipeline_free(&pipeline);

                free(line);

                return 0;
            }
        }

        /*
         * Free pipeline.
         */
        pipeline_free(&pipeline);

        free(line);
    }

    return 0;
}
