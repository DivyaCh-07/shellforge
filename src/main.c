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

int main(void)
{
    char *line;

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

        /* Store command for UP arrow and history */
        add_history(line);

        /* Exit */
        if (strcmp(line, "exit") == 0)
        {
            free(line);
            printf("Exiting...\n");
            break;
        }

        /* Lexer */
        if (!lexer(line, &tokens))
        {
            printf("Lexer failed.\n");
            free(line);
            continue;
        }

        /* Display tokens */
        token_print(&tokens);

        /* Parser */
        if (!parse(&tokens, &pipeline))
        {
            free(line);
            continue;
        }

        /* Variable expansion */
        expand_variables(&pipeline);

        /* Display parsed pipeline */
pipeline_print(&pipeline);

/* Execute commands */
for (int i = 0; i < pipeline.command_count; i++)
{
    int result = execute_command(&pipeline.commands[i]);

    if (result == 1)
    {
        pipeline_free(&pipeline);
        free(line);
        return 0;
    }
}

/* Free pipeline after execution */
pipeline_free(&pipeline);

free(line);
    }

    return 0;
}
