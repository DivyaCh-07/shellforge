#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

#define MAX_COMMANDS 10
#define MAX_ARGS 100

typedef struct
{
    char *argv[MAX_ARGS];
    int argc;

    char input[MAX_TOKEN_LEN];
    char output[MAX_TOKEN_LEN];

    int append;
    int background;

} command_t;

typedef struct
{
    command_t commands[MAX_COMMANDS];
    int command_count;

} pipeline_t;

void command_init(command_t *cmd);

int parse(const token_list_t *tokens, pipeline_t *pipeline);

void pipeline_print(const pipeline_t *pipeline);

void pipeline_free(pipeline_t *pipeline);

#endif
