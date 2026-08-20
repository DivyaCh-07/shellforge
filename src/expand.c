#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "expand.h"
#include "parser.h"

static char *duplicate_string(const char *text)
{
    char *copy;

    if (text == NULL)
    {
        return NULL;
    }

    copy = malloc(strlen(text) + 1);

    if (copy == NULL)
    {
        return NULL;
    }

    strcpy(copy, text);

    return copy;
}

static char *expand_string(const char *text)
{
    char result[MAX_TOKEN_LEN];
    int i = 0;
    int j = 0;

    if (text == NULL)
    {
        return NULL;
    }

    while (text[i] != '\0' && j < MAX_TOKEN_LEN - 1)
    {
        if (text[i] == '$')
        {
            char variable[MAX_TOKEN_LEN];
            const char *value;
            int k = 0;

            i++;

            if (text[i] == '\0')
            {
                result[j++] = '$';
                break;
            }

            while ((isalnum((unsigned char)text[i]) ||
                    text[i] == '_') &&
                   k < MAX_TOKEN_LEN - 1)
            {
                variable[k++] = text[i];
                i++;
            }

            variable[k] = '\0';

            if (k == 0)
            {
                result[j++] = '$';
                continue;
            }

            value = getenv(variable);

            if (value != NULL)
            {
                while (*value != '\0' &&
                       j < MAX_TOKEN_LEN - 1)
                {
                    result[j++] = *value;
                    value++;
                }
            }
        }
        else
        {
            result[j++] = text[i];
            i++;
        }
    }

    result[j] = '\0';

    return duplicate_string(result);
}

void expand_variables(pipeline_t *pipeline)
{
    int i;
    int j;

    if (pipeline == NULL)
    {
        return;
    }

    for (i = 0; i < pipeline->command_count; i++)
    {
        command_t *cmd = &pipeline->commands[i];

        for (j = 0; j < cmd->argc; j++)
        {
            char *expanded = expand_string(cmd->argv[j]);

            if (expanded != NULL)
            {
                free(cmd->argv[j]);
                cmd->argv[j] = expanded;
            }
        }

        if (cmd->input[0] != '\0')
        {
            char *expanded = expand_string(cmd->input);

            if (expanded != NULL)
            {
                strncpy(cmd->input,
                        expanded,
                        MAX_TOKEN_LEN - 1);

                cmd->input[MAX_TOKEN_LEN - 1] = '\0';

                free(expanded);
            }
        }

        if (cmd->output[0] != '\0')
        {
            char *expanded = expand_string(cmd->output);

            if (expanded != NULL)
            {
                strncpy(cmd->output,
                        expanded,
                        MAX_TOKEN_LEN - 1);

                cmd->output[MAX_TOKEN_LEN - 1] = '\0';

                free(expanded);
            }
        }
    }
}
