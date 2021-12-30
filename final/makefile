all: target

target: header.h database.h server.c client.c 
	gcc -pthread -o server server.c header.h database.h
	gcc -o client client.c header.h database.h

clean:	rm target
