#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "token.h"
#include "lexer.h"

int main(void)
{
    char *line;

    printf("=====================================\n");
    printf("Shellforge \n");
    printf(" A Unix Style Shell written in C\n");
    printf("=====================================\n");

    while (1)
    {
        /* Read input from ShellForge */
        line = readline("shellforge$ ");

        /* Ctrl + D */
        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        /* Ignore empty input */
        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }

        /*
         * IMPORTANT:
         * This keeps the UP and DOWN arrow history working.
         */
        add_history(line);

        /* Exit */
        if (strcmp(line, "exit") == 0)
        {
            free(line);
            printf("Exiting...\n");
            break;
        }

        /*
         * HISTORY COMMAND
         */
        if (strcmp(line, "history") == 0)
        {
            HIST_ENTRY **entries;
            int i = 0;

            entries = history_list();

            if (entries != NULL)
            {
                while (entries[i] != NULL)
                {
                    printf("%d  %s\n",
                           i + 1,
                           entries[i]->line);
                    i++;
                }
            }

            free(line);
            continue;
        }

        /*
         * LEXER / TOKENIZER
         */
        token_list_t tokens;

        if (lexer(line, &tokens))
        {
            token_print(&tokens);
        }
        else
        {
            printf("Lexer failed.\n");
        }

        /*
         * YOUR PREVIOUS OUTPUT
         */
        printf(" YOU ENTERED : %s\n", line);

        free(line);
    }

    return 0;
}
