#ifndef __USER_H__
#define __USER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NICK_LENGTH 10
#define USERS_FILE "users.db"

typedef struct u {
	int pipe_read;
	int pipe_write;
	char nick[NICK_LENGTH + 1];
} user;

void addUser(int pipe_read, int pipe_write, char *nick);
void removeUser(char *nick);
void removeAllUsers(void);
int findUserByNick(char *nick);
char* activeUsers(void);
user * getActiveUsers(int * usersCount);
void sendMessage(char * from, char * to, char * message);
void close_all_active_pipes(void);
int prepare_pipe_fds(fd_set * readfds);
void openDB(void);
void updateDB(void);
void closeDB(void);

#endif /* __USER_H__ */
