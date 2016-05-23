#include "user.h"
#include "misc.h"

user *users = NULL;
int cnt_usr = 0;
int uni_fd = -1;

void addUser(int pipe_read, int pipe_write, char *nick)
{
	char buffer[sizeof(user)];
	char message[MAX_BUFFER_LENGTH];
	int i;	
	
	/* Cistenie buffra */
	for (i = 0; i < sizeof(user); i++) {
		buffer[i] = 0;
	}

	openDB();
	if (uni_fd < 0) {
		uni_fd = open(USERS_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (uni_fd < 0) {			
			log_error("Chyba otvorenia súboru.");
		}
	}
	lseek(uni_fd, 0, SEEK_END);
	int written = write(uni_fd, buffer, sizeof(user));
	if (written == -1) {
		log_error("Chyba pri zápise do súboru.");
	}
	closeDB();

	openDB();
	users[cnt_usr - 1].pipe_read = pipe_read;
	users[cnt_usr - 1].pipe_write = pipe_write;
	strcpy(users[cnt_usr - 1].nick, nick);

	closeDB();
	
	sprintf(message, "Používateľ %s pridaný.", nick);
	log_message(LOG_INFO, message);
}

void removeUser(char *nick)
{
	int id = findUserByNick(nick);
	char message[MAX_BUFFER_LENGTH];
	if (id < 0) {
		sprintf(message, "Nepodarilo sa nájsť používateľa %s.", nick);
		log_error(message);
	}

	/* vymením požadovaného používateľa s posledným */
	openDB();
	users[id] = users[cnt_usr - 1];
	user *last = &users[cnt_usr - 1];
	memset(last, 0, sizeof(user));
	closeDB();

	/* skrátim súbor na požadovanú dĺžku */
	openDB();
	cnt_usr--;
	if (ftruncate(uni_fd, sizeof(user) * cnt_usr) == -1) {
		log_error("Problém pri orezávaní súboru.");
	}
	closeDB();

	sprintf(message, "Používateľ %s odstránený.", nick);
	log_message(LOG_INFO, message);
}

void removeAllUsers()
{
	openDB();
	if (cnt_usr) {
		if (ftruncate(uni_fd, 0) == -1) {
			log_error("Problém pri orezávaní súboru.");
		}
	}
	closeDB();
	
	log_message(LOG_INFO, "Odstránení všetci používatelia.");
}

int findUserByNick(char *nick)
{
	if (strncmp(nick, "help", 4) == 0 || strncmp(nick, "list", 4) == 0 || strncmp(nick, "quit", 4) == 0 || strncmp(nick, "new", 3) == 0) {
		return -2;
	}

	openDB();
	for (int i = 0; i < cnt_usr; i++) {
		if (strcmp(users[i].nick, nick) == 0) {
			closeDB();
			return i;
		}
	}
	closeDB();
	return -1;
}

char* activeUsers()
{
	int length;
	openDB();
	char *string = malloc((NICK_LENGTH + 1) * cnt_usr);
	char *stringPointer = string;

	for (int i = 0; i < cnt_usr; i++) {
		length = strlen(users[i].nick);
		strncpy(stringPointer, users[i].nick, length);
		stringPointer += length;
		if (i < cnt_usr - 1) {
			strncpy(stringPointer, ";", 1);
			stringPointer++;
		}
	}
	closeDB();

	return string;
}

user * getActiveUsers(int * usersCount)
{
	user * usersList;
	int i;
	openDB();
	*usersCount = cnt_usr;
	usersList = malloc(sizeof(user) * cnt_usr);
	for (i = 0; i < cnt_usr; i++) {
		usersList[i].pipe_read = users[i].pipe_read;
		usersList[i].pipe_write = users[i].pipe_write;
		strcpy(usersList[i].nick, users[i].nick);
	}
	closeDB();
	return usersList;
}

void sendMessage(char * from, char * to, char * message)
{		
	int i, usersCount;
	char buffer[MAX_BUFFER_LENGTH];
	user * usersList;
	if (to == NULL) {
		usersList = getActiveUsers(&usersCount);
		for (i = 0; i < usersCount; i++) {
			sendMessage(from, usersList[i].nick, message);
		}	
		free(usersList);	
	} else {
		i = findUserByNick(to);
		if (i < 0) {
			i = findUserByNick(from);
			openDB();
			write(users[i].pipe_write, "warning", 7);
			closeDB();
		} else {
			memset(buffer, 0, MAX_BUFFER_LENGTH);

			sprintf(buffer, "%s#%s", from, message);

			openDB();
			write(users[i].pipe_write, buffer, MAX_BUFFER_LENGTH);
			closeDB();
		}
	}
}

void close_all_active_pipes()
{
	int i, usersCount;
	user * usersList;

	usersList = getActiveUsers(&usersCount);

	for (i = 0; i < usersCount; i++) {
		close(usersList[i].pipe_read);
		close(usersList[i].pipe_write);
	}	
	free(usersList);
}

void openDB()
{
	uni_fd = open(USERS_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (uni_fd < 0) {
		log_error("Chyba otvorenia suboru.");
	}
	
	struct stat finfo;
	fstat(uni_fd, &finfo);
	int cusr = (int) finfo.st_size / sizeof(user);
	
	if (finfo.st_size) {		
		users = (user*) mmap(NULL, sizeof(user) * cusr, PROT_READ | PROT_WRITE, MAP_SHARED, uni_fd, 0);
		if (users == MAP_FAILED) {
			close(uni_fd);
			uni_fd = -1;
			log_error("Nepodarilo sa namapovať.");
		}		
		cnt_usr = cusr;
		return;
	}
	close(uni_fd);
	uni_fd = -1;
	cnt_usr = 0;
}

void updateDB()
{
	msync((void*)users, sizeof(user) * cnt_usr, MS_SYNC);
}

void closeDB()
{
	updateDB();
	munmap((void*)users, sizeof(user) * cnt_usr);
	close(uni_fd);
	uni_fd = -1;
	cnt_usr = 0;
}
