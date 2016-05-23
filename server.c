#include "server.h"

static int interrupted = 0;

void interrupt(int signal)
{
	interrupted = 1;
}

void server_quit(int server_socket_fd, int client_socket_fd, char * message)
{
	memset(message, 0, MAX_BUFFER_LENGTH);
	strcpy(message, "server quit");
	if (client_socket_fd) {
		write(client_socket_fd, message, strlen(message));
		close(client_socket_fd);
	}
	if (server_socket_fd)
		close(server_socket_fd);
	close_all_active_pipes();
	closelog();
	exit(EXIT_SUCCESS);
}

void forward_child_to_parent(int pipefd, char * buffer)
{
	write(pipefd, buffer, strlen(buffer));
}

void forward_child_to_client(char * buffer, int client_socket_fd)
{
	write(client_socket_fd, buffer, strlen(buffer));
}

void forward_parent(char * buffer)
{
	char * from, * to, * message;
	char * delimiter = "#";

	from = strtok(buffer, delimiter);
	to = strtok(NULL, delimiter);
	message = strtok(NULL, delimiter);

	if (message == NULL) {
		message = to;
		to = NULL;
	}
	sendMessage(from, to, message);
}

int main(int argc, char *argv[])
{
	int i;
	config conf;
	char buffer[MAX_BUFFER_LENGTH];
	char message[MAX_BUFFER_LENGTH];
	char nick[NICK_LENGTH + 1];
	int server_socket_fd, client_socket_fd;
	socklen_t server_len, client_len;
	int result, errno;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;
	struct timeval timeout;
	fd_set sockset, readfds, master;
	int status = 0;
	int parent_to_child[2];
	int child_to_parent[2];
	int elapsed_sec = 0;
	int fdmax = -1;
	
	errno = 0;
	interrupted = 0;
	FD_ZERO(&master);

	/* otvorenie system logger-u */
	openlog(PROGRAM_TITLE, LOG_PID|LOG_CONS, LOG_USER);

	/* načítanie konfiguráku */
	conf = read_config_file(CONFIG_FILE_NAME);

	/* vytvorenie soketu */
	server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_socket_fd == -1) {
		log_error("Chyba pri vytváraní soketu.");
	}

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(conf.port);
	server_len = sizeof(server_address);

	/* zviazanie soketu s adresou */
	result = bind(server_socket_fd, (struct sockaddr *)&server_address, server_len);	
	if (result == -1) {
		log_error("Chyba pri pripojení soketu k adrese.");
	}

	result = listen(server_socket_fd, LISTEN_BACKLOG);
	if (result == -1) {
		log_error("Chyba pri počúvaní soketu.");
	}

	/* obsluha SIG_CHLD - ignorujem */
	signal(SIGCHLD, SIG_IGN);

	/* obsluha SIG_INT - odstráň všetkých aktívnych používateľov a ukonči */
	struct sigaction act;
	memset (&act, '\0', sizeof(act));
	act.sa_handler = &interrupt;
	sigaction(SIGINT, &act, NULL);

	sprintf(message, "Server beží na porte %d.", conf.port);
	log_message(LOG_INFO, message);

	while(1) {
		/* používateľ prerušil proces */
		if (interrupted) {
			log_message(LOG_INFO, "Zastavujem server...");
			close_all_active_pipes();
			removeAllUsers();
			while (wait(&status) > 0);
			if (server_socket_fd)
				close(server_socket_fd);
			close(parent_to_child[1]);
			close(child_to_parent[0]);
			closelog();
			exit(EXIT_SUCCESS);					
		}

		/* preposlanie čakajúcich správ */
		readfds = master;		
		timeout.tv_usec = 0;
		timeout.tv_sec = 0;
		result = select(fdmax + 1, &readfds, NULL, NULL, &timeout);			
		if (result > 0) {
			/* hľadanie čakajúcich správ */
			for (i = 0; i <= fdmax; i++) {
				if (FD_ISSET(i, &readfds)) {
					memset(buffer, 0, MAX_BUFFER_LENGTH);
					result = read(i, buffer, MAX_BUFFER_LENGTH);
					if (result > 0) {
						if (strncmp(buffer, "quit", 4) == 0) {
							FD_CLR(i, &master);
						} else {		
							forward_parent(buffer);
						}
					}
				}
			}
		}

		/* kontrola, či niekto čaká na vytvorenie spojenia */
		FD_ZERO(&sockset);
		FD_SET(server_socket_fd, &sockset);
		timeout.tv_usec = 500000;
		timeout.tv_sec = 0;
		result = select(server_socket_fd + 1, &sockset, NULL, NULL, &timeout);
		if (result <= 0) {
			/* vypršal časový limit */
			continue;
		}

		/* prijatie spojenia */
		client_len = sizeof(client_address);
		client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&client_address, &client_len);				

		if (pipe(parent_to_child) == -1) {
			log_error("Chyba pri vytváraní rúr.");
		}
		if (pipe(child_to_parent) == -1) {
			log_error("Chyba pri vytváraní rúr.");
		}

		FD_SET(child_to_parent[0], &master);
		if (child_to_parent[0] > fdmax) {
			fdmax = child_to_parent[0];
		}		

		if (fork() == 0) {
			/* child process - spracujem správy */	
			close(parent_to_child[1]);
			close(child_to_parent[0]);
			close_all_active_pipes();
			while(1) {
				/* používateľ prerušil proces - oznám klientom zastavenie serveru */
				if (interrupted) {
					close(parent_to_child[0]);
					close(child_to_parent[1]);
					server_quit(server_socket_fd, client_socket_fd, message);
				}

				/* preposlanie správy klientovi */
				FD_ZERO(&sockset);
				FD_SET(parent_to_child[0], &sockset);
				timeout.tv_usec = 0;
				timeout.tv_sec = 0;
				result = select(parent_to_child[0] + 1, &sockset, NULL, NULL, &timeout);
				if (result > 0) {
					memset(buffer, 0, MAX_BUFFER_LENGTH);
					result = read(parent_to_child[0], buffer, MAX_BUFFER_LENGTH);
					forward_child_to_client(buffer, client_socket_fd);
				}

				/* čakám na správu od klienta */
				if (strcmp(nick, "") != 0) {					
					FD_ZERO(&sockset);
					FD_SET(client_socket_fd, &sockset);
					timeout.tv_usec = 0;
					timeout.tv_sec = 1;
					result = select(client_socket_fd + 1, &sockset, NULL, NULL, &timeout);
					if (result <= 0) {
						elapsed_sec++;
						if (elapsed_sec >= conf.timeout_sec) {
							/* vypršal časový limit - server quit */
							removeUser(nick);
							server_quit(server_socket_fd, client_socket_fd, message);
						}
						continue;
					}
				}

				elapsed_sec = 0;
				
				memset(buffer, 0, MAX_BUFFER_LENGTH);
				read(client_socket_fd, buffer, MAX_BUFFER_LENGTH);

				/* kontrola nick-u */
				if (strncmp(buffer, "check", 5) == 0) {				
					strcpy(nick, buffer + 6);
					sprintf(message, "Nick: %s", nick);
					log_message(LOG_DEBUG, message);
					if (findUserByNick(nick) == -1) {
						addUser(child_to_parent[0], parent_to_child[1], nick);
						write(client_socket_fd, "ok", 2);
					} else {
						write(client_socket_fd, "no", 2);
						strcpy(nick, "");
					}
					continue;
				}
	
				/* odoslanie zoznamu aktívnych používateľov */
				if (strncmp(buffer, "list", 4) == 0) {
					log_message(LOG_DEBUG, "list");
					write(client_socket_fd, activeUsers(), MAX_BUFFER_LENGTH);
					continue;
				}

				/* odstránenie používateľa spomedzi aktívnych */
				if (strncmp(buffer, "quit", 4) == 0) {
					strcpy(nick, buffer + 5);
					removeUser(nick);
					forward_child_to_parent(child_to_parent[1], buffer);
					break;
				}

				/* preposlanie správy */
				forward_child_to_parent(child_to_parent[1], buffer);
			}

			if (client_socket_fd)
				close(client_socket_fd);
			if (server_socket_fd)
				close(server_socket_fd);
			close(parent_to_child[0]);
			close(child_to_parent[1]);
			closelog();
			exit(EXIT_SUCCESS);
		} else {
			/* parent process - čakám ďalej */
			close(client_socket_fd);
			close(parent_to_child[0]);
			close(child_to_parent[1]);
		}
	}
	close(parent_to_child[1]);
	close(child_to_parent[0]);	
	close(server_socket_fd);
	closelog();
	exit(EXIT_SUCCESS);
}
