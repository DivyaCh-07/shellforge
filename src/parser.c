#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "token.h"

static char *copy_string(const char *text)
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

void command_init(command_t *cmd)
{
    int i;

    if (cmd == NULL)
    {
        return;
    }

    cmd->argc = 0;

    cmd->input[0] = '\0';
    cmd->output[0] = '\0';

    cmd->append = 0;
    cmd->background = 0;

    for (i = 0; i < MAX_ARGS; i++)
    {
        cmd->argv[i] = NULL;
    }
}

int parse(const token_list_t *tokens, pipeline_t *pipeline)
{
    int i;
    int current = 0;

    if (tokens == NULL || pipeline == NULL)
    {
        return 0;
    }

    pipeline->command_count = 1;

    for (i = 0; i < MAX_COMMANDS; i++)
    {
        command_init(&pipeline->commands[i]);
    }

    i = 0;

    while (i < tokens->count)
    {
        const token_t *token = &tokens->tokens[i];
        command_t *cmd = &pipeline->commands[current];

        /* Normal command/argument */
        if (token->type == TOKEN_WORD)
        {
            if (cmd->argc >= MAX_ARGS - 1)
            {
                printf("Too many arguments.\n");
                pipeline_free(pipeline);
                return 0;
            }

            cmd->argv[cmd->argc] = copy_string(token->text);

            if (cmd->argv[cmd->argc] == NULL)
            {
                pipeline_free(pipeline);
                return 0;
            }

            cmd->argc++;
            i++;
        }

        /* Input redirection */
        else if (token->type == TOKEN_INPUT)
        {
            if (i + 1 >= tokens->count ||
                tokens->tokens[i + 1].type != TOKEN_WORD)
            {
                printf("filename expected after <\n");
                pipeline_free(pipeline);
                return 0;
            }

            strncpy(cmd->input,
                    tokens->tokens[i + 1].text,
                    MAX_TOKEN_LEN - 1);

            cmd->input[MAX_TOKEN_LEN - 1] = '\0';

            i += 2;
        }

        /* Output redirection */
        else if (token->type == TOKEN_OUTPUT)
        {
            if (i + 1 >= tokens->count ||
                tokens->tokens[i + 1].type != TOKEN_WORD)
            {
                printf("filename expected after >\n");
                pipeline_free(pipeline);
                return 0;
            }

            strncpy(cmd->output,
                    tokens->tokens[i + 1].text,
                    MAX_TOKEN_LEN - 1);

            cmd->output[MAX_TOKEN_LEN - 1] = '\0';

            cmd->append = 0;

            i += 2;
        }

        /* Append redirection */
        else if (token->type == TOKEN_APPEND)
        {
            if (i + 1 >= tokens->count ||
                tokens->tokens[i + 1].type != TOKEN_WORD)
            {
                printf("filename expected after >>\n");
                pipeline_free(pipeline);
                return 0;
            }

            strncpy(cmd->output,
                    tokens->tokens[i + 1].text,
                    MAX_TOKEN_LEN - 1);

            cmd->output[MAX_TOKEN_LEN - 1] = '\0';

            cmd->append = 1;

            i += 2;
        }

        /* Background execution */
        else if (token->type == TOKEN_BACKGROUND)
        {
            cmd->background = 1;
            i++;
        }

        /* Pipe */
        else if (token->type == TOKEN_PIPE)
        {
            if (current >= MAX_COMMANDS - 1)
            {
                printf("Too many commands in pipeline.\n");
                pipeline_free(pipeline);
                return 0;
            }

            current++;

            pipeline->command_count++;

            command_init(&pipeline->commands[current]);

            i++;
        }

        /* End of command */
        else if (token->type == TOKEN_END)
        {
            break;
        }

        else
        {
            i++;
        }
    }

    /*
     * NULL terminate argv for every command.
     * This is required by execvp().
     */
    for (i = 0; i < pipeline->command_count; i++)
    {
        pipeline->commands[i]
            .argv[pipeline->commands[i].argc] = NULL;
    }

    return 1;
}

void pipeline_print(const pipeline_t *pipeline)
{
    int i;
    int j;

    if (pipeline == NULL)
    {
        return;
    }

    printf("\n========== PIPELINE ==========\n\n");

    for (i = 0; i < pipeline->command_count; i++)
    {
        const command_t *cmd = &pipeline->commands[i];

        printf("Command %d\n", i + 1);
        printf("-----------------------------\n");

        printf("Arguments\n");

        for (j = 0; j < cmd->argc; j++)
        {
            printf("argv[%d] = %s\n",
                   j,
                   cmd->argv[j]);
        }

        printf("Input     : %s\n",
               cmd->input[0] ? cmd->input : "None");

        printf("Output    : %s\n",
               cmd->output[0] ? cmd->output : "None");

        printf("Append    : %s\n",
               cmd->append ? "Yes" : "No");

        printf("Background : %s\n",
               cmd->background ? "Yes" : "No");

        printf("=============================\n");
    }
}

void pipeline_free(pipeline_t *pipeline)
{
    int i;
    int j;

    if (pipeline == NULL)
    {
        return;
    }

    for (i = 0; i < pipeline->command_count; i++)
    {
        for (j = 0; j < pipeline->commands[i].argc; j++)
        {
            free(pipeline->commands[i].argv[j]);
            pipeline->commands[i].argv[j] = NULL;
        }

        pipeline->commands[i].argc = 0;
    }

    pipeline->command_count = 0;
}
