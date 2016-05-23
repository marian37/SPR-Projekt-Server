#ifndef __SERVER_H__
#define __SERVER_H__

#include "user.h"
#include "misc.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <wait.h>

#define PROGRAM_TITLE "chat-server"
#define CONFIG_FILE_NAME "server.conf"
#define LISTEN_BACKLOG 5
#define INPUT_LENGTH 5

void interrupt(int signal);
void server_quit(int server_socket_fd, int client_socket_fd, char * message);
void forward_child_to_parent(int pipefd, char * buffer);
void forward_child_to_client(char * buffer, int client_socket_fd);
void forward_parent(char * buffer);

#endif /* __SERVER_H__ */
