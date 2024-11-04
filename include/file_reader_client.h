//
// Created by blaise-klein on 9/30/24.
//

#ifndef FILE_READER_CLIENT_H
#define FILE_READER_CLIENT_H
#include "open.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sys/fcntl.h>
#include <unistd.h>

// static const unsigned long FIFONAMEMAXLENGTH = 128;

enum Text_Modifications
{
    lower   = 0,
    upper   = 1,
    none    = 2,
    invalid = 3
};

struct arguments
{
    int                     argc;
    char                   *ip_target;
    in_port_t               target_port;
    char                   *input;
    size_t                  input_length;
    char                   *output;
    enum Text_Modifications modification_option;
    char                    option_char;
    char                  **argv;
};

struct context
{
    struct arguments *arguments;
    int               server_fd;
    int               err;
};

void cleanup_context(struct context *ctx);

void cleanup_file(int fd);

void cleanup_string(char *str);

noreturn void usage(const char *err);

void parse_arguments(int argc, char **argv, struct context *ctx);

void get_modification_char(struct context *ctx);

size_t write_to_server(struct context *ctx);

ssize_t await_server_response(struct context *ctx);

void modify_text_input(struct context *ctx);

void test_fifo(const char *filename, const char *msg, size_t msg_size);

int main(int argc, char **argv);

static in_port_t convert_port(const char *str, int *err);

#endif    // FILE_READER_CLIENT_H
