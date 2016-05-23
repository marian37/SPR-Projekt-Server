#include "misc.h"

config read_config_file(char* file_name)
{
	FILE * file;
	config conf; 
	char buffer[SMALL_BUFFER_LENGTH];

	file = fopen(file_name, "r");
	if (file == NULL) {
		log_error("Chyba pri čítaní súboru.");
	}
	
	fgets(buffer, SMALL_BUFFER_LENGTH + 1, file);
	conf.port = atoi(buffer);
	fgets(buffer, SMALL_BUFFER_LENGTH + 1, file);
	conf.timeout_sec = atoi(buffer);

	fclose(file);
	return conf;
}

void log_message(int priority, char * message) 
{
	printf("%s\n", message);
	syslog(priority, "%s - %m\n", message);
}

void log_error(char * message)
{
	log_message(LOG_ERR, message);
	exit(EXIT_FAILURE);
}
