#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "header.h"
#include "database.h"


int main(int argc, char *argv[]){
	int sfd, port, id,
		op, lineId,
		i, j, execCount = 0;
	char *ip, *queryfpath, *line = NULL, ***retVal = NULL;
	FILE *fp;
	struct sockaddr_in servaddr;
	size_t lineSize = 0, strSize;
	int rowSize, colSize;

// arg control
	while ((op = getopt(argc, argv, ":i:a:p:o:")) != -1){
		switch (op) {
			case 'i':
				id = atoi(optarg);
				break;
			case 'a':
				ip = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'o':
				queryfpath = optarg;
				break;
			case ':':
				printf("Every command line argument option needs a value.\n");
				exit(1);
				break;
			case '?':
				printf("Can only have parameters with -i, -a, -p, -o.\n");
				exit(1);
				break;
		}
	}

	if ((fp = fopen(queryfpath,"r")) == NULL) exitprint(1,"client fopen(query file)");

	printf("Client-%d connecting to %s:%d\n", id, ip, port);

// socket create
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) exitprint(1, "client socket");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip);
	servaddr.sin_port = htons(port);

// socket connect
	if (connect(sfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) exitprint(1, "client connect");

// read file
	while (!feof(fp)) {
		//read line
		fscanf(fp, "%d ", &lineId);
		//get line initializes and assignes the size to the variables
		getline(&line,&lineSize,fp);
		//query of this client read from file
		if (lineId == id) {
			//remove \t \n chars from line
			lineSize = strlen(line) - 2;
			line[lineSize] = '\0';
			printf("Client-%d connected and sending query '%s'\n", id, line);
			//send size of query
			write(sfd, &lineSize, sizeof(lineSize));
			//send query
			write(sfd, line, lineSize);
			
			//read size of the result
			read(sfd, &rowSize, sizeof(rowSize));
			/*
			read(sfd, &colSize, sizeof(colSize));
			//allocate space for result
			retVal = malloc(rowSize * sizeof(char **));
			for(i = 0; i < rowSize; ++i)
				retVal[i] = malloc(colSize * sizeof(char *));
			
			//read results
			for (i = 0; i < rowSize; ++i) {
				for (j = 0; j < colSize; ++j) {
					//read string size
					read(sfd, &strSize, sizeof(strSize));
					//allocate space for string
					retVal[i][j] = malloc(strSize * sizeof(char));
					//read string
					read(sfd, retVal[i][j], strSize);
				}
			}
			*/
			execCount++;

			//print the results
			if (rowSize != 0)	
				printf("Server's response to Client-%d is %d records\n", id, rowSize-1);
			else
				printf("Server's response to Client-%d is %d records\n", id, rowSize);
			if (rowSize > 1) {
				for (i = 0; i < rowSize; ++i) {
					if (i != 0)
						printf("%d\t", i);
					for (j = 0; j < colSize; ++j)
						printf("%s\t\t", retVal[i][j]);
					printf("\n");
				}
			}

			//free allocated space for result
			if (retVal != NULL) {
				for (i = 0; i < rowSize; ++i) {
					for (j = 0; j < colSize; ++j)
						free(retVal[i][j]);
					free(retVal[i]);
				}
				free(retVal);
			}
		}
		//reset vars
		if (line != NULL )
			free(line);
		lineSize = 0;
	}
	printf("A total of %d queries were executed, client-%d is terminating\n", execCount, id);

	//message the server that the connection is over
	write(sfd,&lineSize,sizeof(lineSize));

	fclose(fp);
	close(sfd);

	return 0;
}
