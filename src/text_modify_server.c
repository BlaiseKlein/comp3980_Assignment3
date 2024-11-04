//
// Created by blaise-klein on 9/30/24.
//
#include "text_modify_server.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 5
#define ERR_NONE 0
#define ERR_NO_DIGITS 1
#define ERR_OUT_OF_RANGE 2
#define ERR_INVALID_CHARS 3
#define SERVER_ERR (-50)

ssize_t send_modified_text(struct thread_arguments *arguments)
{
    ssize_t bytes_written = 0;

    bytes_written = write(arguments->out_fd, arguments->msg, arguments->msg_length);
    close(arguments->out_fd);
    printf("sent\n");
    if(bytes_written == -1)
    {
        // perror("Failed to send data over /tmp/response_fifo");
        free(arguments->msg);
        free(arguments);
        usage("Failed to send data over /tmp/response_fifo");
    }

    return bytes_written;
}

static int accept_connection(const struct sockaddr_storage *addr, socklen_t addr_len, int backlog, int *err)
{
    int server_fd;
    int result;

    server_fd = socket(addr->ss_family, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)

    if(server_fd == -1)
    {
        *err = errno;
        goto done;
    }

    result = bind(server_fd, (const struct sockaddr *)addr, addr_len);

    if(result == -1)
    {
        *err = errno;
        goto done;
    }

    result = listen(server_fd, backlog);

    if(result == -1)
    {
        *err = errno;
        goto done;
    }
    return server_fd;

done:
    close(server_fd);
    server_fd = -1;

    return server_fd;
}

int open_network_socket_server(const char *address, in_port_t port, int backlog, int *err)
{
    struct sockaddr_storage addr;
    socklen_t               addr_len;

    setup_network_address(&addr, &addr_len, address, port, err);
    if(*err == SERVER_ERR)
    {
        return -1;
    }

    return accept_connection(&addr, addr_len, backlog, err);
}

int main(int argc, char **argv)
{
    // int          response_test_fd;
    struct server_context *ctx;

    if(signal(SIGINT, handle_signal) == SIG_ERR)
    {
        perror("Error setting up signal handler");
        return 1;
    }

    ctx = (struct server_context *)malloc(sizeof(struct server_context));
    if(ctx == NULL)
    {
        usage("Failed to allocate memory for server context");
    }
    parse_arguments(argc, argv, ctx);
    // response_test_fd = open(fifo_out, O_WRONLY | O_CLOEXEC);
    // if(response_test_fd == -1)
    // {
    //     perror("Failed to open /tmp/response_fifo");
    //     exit(1);
    // }
    // close(response_test_fd);

    // printf("starting server\n");
    await_fifo_connection(ctx);
}

static void *handle_fifo_connection(void *args)
{
    struct thread_arguments *arguments = (struct thread_arguments *)args;
    // printf("%c", arguments->msg[0]);

    for(size_t i = 0; i < arguments->msg_length; i++)
    {
        arguments->msg[i] = char_mods[arguments->option](arguments->msg[i]);
    }

    send_modified_text(arguments);
    free(arguments->msg);
    free(arguments);
    arguments = NULL;
    return NULL;
}

enum Text_Modifications option_character(char c)
{
    if(c == 'l')
    {
        return lower;
    }
    if(c == 'u')
    {
        return upper;
    }
    if(c == 'n')
    {
        return none;
    }
    exit(EXIT_FAILURE);
}

void setup_network_address(struct sockaddr_storage *addr, socklen_t *addr_len, const char *address, in_port_t port, int *err)
{
    in_port_t net_port;

    *addr_len = 0;
    net_port  = htons(port);
    memset(addr, 0, sizeof(*addr));

    if(inet_pton(AF_INET, address, &(((struct sockaddr_in *)addr)->sin_addr)) == 1)
    {
        struct sockaddr_in *ipv4_addr;

        ipv4_addr           = (struct sockaddr_in *)addr;
        addr->ss_family     = AF_INET;
        ipv4_addr->sin_port = net_port;
        *addr_len           = sizeof(struct sockaddr_in);
    }
    else if(inet_pton(AF_INET6, address, &(((struct sockaddr_in6 *)addr)->sin6_addr)) == 1)
    {
        struct sockaddr_in6 *ipv6_addr;

        ipv6_addr            = (struct sockaddr_in6 *)addr;
        addr->ss_family      = AF_INET6;
        ipv6_addr->sin6_port = net_port;
        *addr_len            = sizeof(struct sockaddr_in6);
    }
    else
    {
        fprintf(stderr, "%s is not an IPv4 or an IPv6 address\n", address);
        *err = SERVER_ERR;
    }
}

noreturn void await_fifo_connection(struct server_context *ctx)
{
    // struct sockaddr_storage addr;
    // socklen_t               addr_len;
    ssize_t bytes_read;
    size_t  msg_size;
    char   *msg_buffer;
    int     server_fd;

    int                      err         = 0;
    size_t                   buffer      = 0;
    struct thread_arguments *thread_args = NULL;

    // fd         = open(filename, O_RDONLY | O_CLOEXEC);
    //
    //
    // if(fd == -1)
    // {
    //     perror("Failed to open /tmp/write_fifofile");
    //     exit(1);
    // }

    // setup_network_address(&addr, &addr_len, ctx->ip_address, ctx->port, &err);
    server_fd = open_network_socket_server(ctx->ip_address, ctx->port, BACKLOG, &err);

    if(server_fd == -1)
    {
        cleanup_context(ctx);
        usage("Server ip is invalid");
    }

    while(true)
    {
        int     client_fd;
        int     pid;
        ssize_t first_read = 0;
        client_fd          = accept(server_fd, NULL, 0);

        if(client_fd == -1)
        {
            close(server_fd);
            cleanup_context(ctx);
            usage("Client connected failed");
        }
        // fork here
        pid = fork();

        if(pid == -1)
        {
            cleanup_context(ctx);
            usage("Error creating child process: Closing connection");
        }

        if(pid == 0)
        {
            // This is the child process
            // printf("Child process finished %d.\n", instance);
            close(server_fd);
        }
        else
        {
            // This is the parent process
            // printf("Parent process finished.\n");
            close(client_fd);
            continue;
        }

        first_read = read(client_fd, &buffer, sizeof(size_t));
        // printf("here\n");
        if(first_read == -1)
        {
            cleanup_context(ctx);
            usage("Failed to read from /tmp/write_fifo");
        }
        // ERR check here

        if(first_read == sizeof(size_t))
        {
            msg_size   = buffer;
            msg_buffer = (char *)malloc(msg_size * sizeof(char) + 2);
            // printf("here");
            if(msg_buffer == NULL)
            {
                cleanup_context(ctx);
                usage("Failed to allocate buffer");
            }
            // Err check here
            // printf("here");

            bytes_read = read(client_fd, msg_buffer, sizeof(char) * msg_size + 2);
            if(bytes_read == -1)
            {
                cleanup_context(ctx);
                free(msg_buffer);
                usage("Failed to read from /tmp/write_fifo");
            }
            // printf("%s", msg_buffer);
            // Err check here
            thread_args = (struct thread_arguments *)malloc(sizeof(struct thread_arguments));
            if(thread_args == NULL)
            {
                cleanup_context(ctx);
                free(msg_buffer);
                usage("Failed to allocate thread arguments");
                exit(1);
            }
            thread_args->msg = (char *)malloc(msg_size * sizeof(char));
            if(thread_args->msg == NULL)
            {
                cleanup_context(ctx);
                free(msg_buffer);
                free(thread_args);
                usage("Failed to allocate thread arguments");
            }
            strlcpy(thread_args->msg, msg_buffer + 1, msg_size + 1);
            thread_args->msg_length = msg_size;
            thread_args->option     = option_character(msg_buffer[0]);
            thread_args->out_fd     = client_fd;

            if(bytes_read == (ssize_t)buffer + 1)
            {
                printf("handling\n");
                handle_fifo_connection(thread_args);
                free(msg_buffer);
                msg_buffer = NULL;
                // free(thread_args->msg);
                // free(thread_args->fifo_out);
                // free(thread_args);
                // thread_args = NULL;
                exit(1);
            }
            else
            {
                // printf("but why, %zd, %zd", first_read, bytes_read);
                free(msg_buffer);
                msg_buffer = NULL;
                free(thread_args->msg);
                free(thread_args);
                thread_args = NULL;
                exit(1);
            }
        }
        if(pid == 0)
        {
            close(client_fd);
            cleanup_context(ctx);
            exit(EXIT_SUCCESS);
        }
    }
    // close(fd);
}

void handle_signal(int signal)
{
    if(signal == SIGINT)
    {
        abort();
    }
}

void cleanup_context(struct server_context *ctx)
{
    if(ctx != NULL)
    {
        // if(ctx->ip_address != NULL)
        // {
        //     // free(ctx->ip_address);
        //     ctx->ip_address = NULL;
        // }
    }
    free(ctx);
}

noreturn void usage(const char *err)
{
    fprintf(stderr, "%s\nUsage: main.c -s ip_address -p port\n", err);
    fprintf(stderr,
            "-s ip_address: Enter the target ip address\n"
            "-p port: Enter the target port\n"
            "-f [lower/upper/none]: Enter the character modification option.\n");
    exit(EXIT_FAILURE);
}

static in_port_t convert_port(const char *str, int *err)
{
    in_port_t port;
    char     *endptr;
    long      val;

    *err  = ERR_NONE;
    port  = 0;
    errno = 0;
    val   = strtol(str, &endptr, 10);    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    // Check if no digits were found
    if(endptr == str)
    {
        *err = ERR_NO_DIGITS;
        goto done;
    }

    // Check for out-of-range errors
    if(val < 0 || val > UINT16_MAX)
    {
        *err = ERR_OUT_OF_RANGE;
        goto done;
    }

    // Check for trailing invalid characters
    if(*endptr != '\0')
    {
        *err = ERR_INVALID_CHARS;
        goto done;
    }

    port = (in_port_t)val;

done:
    return port;
}

void parse_arguments(int argc, char **argv, struct server_context *ctx)
{
    int       opt;
    const int valid_argument_count = 5;
    const int help_argument_count  = 2;

    // struct context* ctx = malloc(sizeof(struct context));
    if(ctx == NULL)
    {
        usage("Called to get context");
    }
    if(argc != valid_argument_count && argc != help_argument_count)
    {
        ctx = NULL;
        cleanup_context(ctx);
        usage("Wrong number of arguments");
    }

    ctx->ip_address = NULL;
    ctx->port       = 0;

    opterr = 0;

    while((opt = getopt(argc, argv, "hs:p:")) != -1)
    {
        switch(opt)
        {
            case 'h':
            {
                usage("Printing usage instructions");
            }

            case 's':
            {
                ctx->ip_address = optarg;
                if(ctx->ip_address == NULL)
                {
                    cleanup_context(ctx);
                    usage("IP target string allocation failed");
                }
                break;
            }
            case 'p':
            {
                ctx->port = convert_port(optarg, &ctx->err);
                break;
            }
            case '?':
            {
                if(optopt == 'i')
                {
                    // cleanup_string(ctx->arguments->output);
                    cleanup_context(ctx);
                    usage("No input string");
                }
                if(optopt == 's')
                {
                    // cleanup_string(ctx->arguments->output);
                    cleanup_context(ctx);
                    usage("No ip address string");
                }
                if(optopt == 'p')
                {
                    // cleanup_string(ctx->arguments->output);
                    cleanup_context(ctx);
                    usage("No port string");
                }
                break;
            }
            default:
            {
                cleanup_context(ctx);
                usage("Invalid option");
            }
        }
    }
    if(ctx->ip_address == NULL)
    {
        cleanup_context(ctx);
        usage("No ip input string");
    }
    if(ctx->port == 0)
    {
        cleanup_context(ctx);
        usage("No port string");
    }
}
