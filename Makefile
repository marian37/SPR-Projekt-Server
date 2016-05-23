CC = gcc

INC = .

FLAGS = -Wall -ansi -pedantic -std=gnu99
OUTPUT = -o
STATIC = -static

RM = rm -f

#--------------------------------

all: server

server: server.o user.o misc.o
	$(CC) server.o user.o misc.o $(OUTPUT) server $(STATIC)

server.o: server.c server.h
	$(CC) $(FLAGS) -I$(INC) -c server.c

user.o: user.c user.h
	$(CC) $(FLAGS) -I$(INC) -c user.c

misc.o: misc.c misc.h
	$(CC) $(FLAGS) -I$(INC) -c misc.c

clean:
	-@$(RM) users.db user.o misc.o server.o server

run:
	@echo "Running program...";
	@./server
