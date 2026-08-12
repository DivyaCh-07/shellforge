#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"
#include "token.h"

int lexer(const char *input, token_list_t *list)
{
    int i = 0;

    if (input == NULL || list == NULL)
    {
        return 0;
    }

    token_list_init(list);

    while (input[i] != '\0')
    {
        char word[MAX_TOKEN_LEN];
        int j = 0;

        /* Skip spaces and tabs */
        if (isspace((unsigned char)input[i]))
        {
            i++;
            continue;
        }

        /* Pipe */
        if (input[i] == '|')
        {
            if (!token_add(list, TOKEN_PIPE, "|"))
            {
                return 0;
            }

            i++;
            continue;
        }

        /* Input redirection */
        if (input[i] == '<')
        {
            if (!token_add(list, TOKEN_INPUT, "<"))
            {
                return 0;
            }

            i++;
            continue;
        }

        /* Output redirection and append */
        if (input[i] == '>')
        {
            if (input[i + 1] == '>')
            {
                if (!token_add(list, TOKEN_APPEND, ">>"))
                {
                    return 0;
                }

                i += 2;
            }
            else
            {
                if (!token_add(list, TOKEN_OUTPUT, ">"))
                {
                    return 0;
                }

                i++;
            }

            continue;
        }

        /* Background */
        if (input[i] == '&')
        {
            if (!token_add(list, TOKEN_BACKGROUND, "&"))
            {
                return 0;
            }

            i++;
            continue;
        }

        /*
         * Read a WORD
         */
        while (input[i] != '\0' &&
               !isspace((unsigned char)input[i]) &&
               input[i] != '|' &&
               input[i] != '<' &&
               input[i] != '>' &&
               input[i] != '&')
        {
            /* Backslash escape */
            if (input[i] == '\\')
            {
                i++;

                if (input[i] == '\0')
                {
                    break;
                }

                if (j < MAX_TOKEN_LEN - 1)
                {
                    word[j++] = input[i];
                }

                i++;
                continue;
            }

            /* Single quotes */
            if (input[i] == '\'')
            {
                i++;

                while (input[i] != '\0' &&
                       input[i] != '\'')
                {
                    if (j < MAX_TOKEN_LEN - 1)
                    {
                        word[j++] = input[i];
                    }

                    i++;
                }

                if (input[i] == '\0')
                {
                    fprintf(stderr,
                            "Lexer Error: Unterminated single quote\n");

                    return 0;
                }

                i++;
                continue;
            }

            /* Double quotes */
            if (input[i] == '"')
            {
                i++;

                while (input[i] != '\0' &&
                       input[i] != '"')
                {
                    if (input[i] == '\\' &&
                        input[i + 1] != '\0')
                    {
                        i++;

                        if (j < MAX_TOKEN_LEN - 1)
                        {
                            word[j++] = input[i];
                        }

                        i++;
                    }
                    else
                    {
                        if (j < MAX_TOKEN_LEN - 1)
                        {
                            word[j++] = input[i];
                        }

                        i++;
                    }
                }

                if (input[i] == '\0')
                {
                    fprintf(stderr,
                            "Lexer Error: Unterminated double quote\n");

                    return 0;
                }

                i++;
                continue;
            }

            /* Normal character */
            if (j < MAX_TOKEN_LEN - 1)
            {
                word[j++] = input[i];
            }

            i++;
        }

        word[j] = '\0';

        if (j > 0)
        {
            if (!token_add(list, TOKEN_WORD, word))
            {
                return 0;
            }
        }
    }

    /* Add END token */
    if (!token_add(list, TOKEN_END, "END"))
    {
        return 0;
    }

    return 1;
}

