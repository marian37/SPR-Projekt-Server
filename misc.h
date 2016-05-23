#ifndef __MISC_H__
#define __MISC_H__

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#define SMALL_BUFFER_LENGTH 10
#define MAX_BUFFER_LENGTH 1024

typedef struct c {
	int port;
	int timeout_sec;
} config;

config read_config_file(char* file_name);
void log_message(int priority, char * message);
void log_error(char * message);

#endif /* __MISC_H__ */
