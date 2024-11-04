//
// Created by blaise-klein on 9/30/24.
//

#ifndef TEXT_MODIFY_SERVER_H
#define TEXT_MODIFY_SERVER_H
#include "character_modifications.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

enum Text_Modifications
{
    lower   = 0,
    upper   = 1,
    none    = 2,
    invalid = 3
};

struct thread_arguments
{
    enum Text_Modifications option;
    char                   *msg;
    int                     out_fd;
    size_t                  msg_length;
};

struct server_context
{
    char     *ip_address;
    in_port_t port;
    int       err;
};

static const operation_func char_mods[] = {text_to_lowercase, text_to_uppercase, text_unmodified};

ssize_t send_modified_text(struct thread_arguments *arguments);

int open_network_socket_server(const char *address, in_port_t port, int backlog, int *err);

static void *handle_fifo_connection(void *args);

void await_fifo_connection(struct server_context *ctx);

void handle_signal(int signal);

void cleanup_context(struct server_context *ctx);

void usage(const char *err);

static in_port_t convert_port(const char *str, int *err);

void setup_network_address(struct sockaddr_storage *addr, socklen_t *addr_len, const char *address, in_port_t port, int *err);

void parse_arguments(int argc, char **argv, struct server_context *ctx);

enum Text_Modifications option_character(char c);

// void basic_fifo_connection(const char *filename);

int main(int argc, char **argv);

#endif    // TEXT_MODIFY_SERVER_H
