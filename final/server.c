#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "header.h"
#include "database.h"

// sig handler
sig_atomic_t sigCtrl = 0;

void sigHandler(int sigNum) {
	sigCtrl = 1;
}


// monitors
pthread_mutex_t queueMutex;
pthread_cond_t queueCond;
int taskCount = 0;
pthread_mutex_t dbMutex;
pthread_cond_t writeCond, readCond;
int WR = 0, WW = 0, AR = 0, AW = 0;
pthread_mutex_t logMutex;

// global vars
struct Queue sfdQ; //socket fd queue
FILE *logfp;


char *** datasetReader(char **args, int *argSize, int *retSize, char command) {
	char ***retVal = NULL;

	//control if db is available
	if (pthread_mutex_lock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");
	while (WW + AW > 0) {
		WR++;
		if (pthread_cond_wait(&readCond,&dbMutex) != 0) exitprint(1, "COND ERR");
		WR--;
	}
	AR++;
	if (pthread_mutex_unlock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");

	//db work
	switch (command) {
		case 'S':
			//retVal = databaseSelect(args, argSize, retSize);
			break;
		case 'D':
			//retVal = databaseSelectDist(args, argSize, retSize);
			break;
	}
	usleep(500000);

	//signal waiting writers
	if (pthread_mutex_lock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");
	AR--;
	if (AR == 0 && WW > 0)
		pthread_cond_signal(&writeCond);
	if (pthread_mutex_unlock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");

	return retVal;
}


char *** datasetWriter(char **args, int *argSize, int *retSize) {
	char ***retVal = NULL;
	//control if db is available
	if (pthread_mutex_lock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");
	while (AR + AW > 0) {
		WW++;
		if (pthread_cond_wait(&writeCond,&dbMutex) != 0) exitprint(1, "COND ERR");
		WW--;
	}
	AW++;
	if (pthread_mutex_unlock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");

	//db work
	//retVal = databaseUpdate(args, argSize, retSize);
	usleep(500000);

	//signal waiting writers
	if (pthread_mutex_lock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");
	AW--;
	if (WW > 0)
		pthread_cond_signal(&writeCond);
	//signal waiting readers
	else if (WR > 0)
		pthread_cond_signal(&readCond);
	if (pthread_mutex_unlock(&dbMutex) != 0) exitprint(1, "MUTEX ERR");

	return retVal;
}


void * poolRoutine(void * arg) {
	int id = *((int *) arg), i, j, sfd, argSize, retSize = 0;
	char str[256], **args, command, ***retVal = NULL;
	size_t strSize = 0;

	while (!sigCtrl) {
		retVal = NULL;
		pthread_mutex_lock(&logMutex);
		fprintf(logfp, "Thread #%d: waiting for connection\n", id);
		pthread_mutex_unlock(&logMutex);
		
		//wait crit sec
		if (pthread_mutex_lock(&queueMutex) != 0) exitprint(1, "MUTEX ERR");
		//wait for new query
		while (taskCount == 0 && !sigCtrl)
			if (pthread_cond_wait(&queueCond, &queueMutex) != 0) exitprint(1, "COND ERR");
		//if signal did not arrived
		if (!sigCtrl) {
			//get query from queue
			sfd = QueueRemove(&sfdQ);
			taskCount--;
			
			pthread_mutex_lock(&logMutex);
			fprintf(logfp, "A connection has been delegated to thread id #%d\n", id);
			pthread_mutex_unlock(&logMutex);
			
			if (pthread_mutex_unlock(&queueMutex) != 0) exitprint(1, "MUTEX ERR");
			
			//handle queries of the client
			while (!sigCtrl) {
			//get query from the client
				read(sfd,&strSize,sizeof(strSize));
				/*if incoming message is 0 rather than a string size, then there is no more query left to send*/
				if (strSize == 0)
					break;
				
				strcpy(str,"");
				if (read(sfd,&str,strSize) != strSize) exitprint(1, "SERVER READ!!!!");
				
				pthread_mutex_lock(&logMutex);
				fprintf(logfp, "Thread #%d: received query '%s'\n", id, str);
				pthread_mutex_unlock(&logMutex);
			
			//parse query
				args = parseQuery(str, &command, &argSize);
			
			//exec query
				switch (command) {
					case 'S':
					case 'D':
						retVal = datasetReader(args, &argSize, &retSize, command);
						break;
					case 'U':
						retVal = datasetWriter(args, &argSize, &retSize);
						break;
					default:
						exitprint(1, "wrong type for query\n");
						break;
				}

				pthread_mutex_lock(&logMutex);
				fprintf(logfp, "Thread %d: query completed, %d records have been returned.\n", id, retSize);
				pthread_mutex_unlock(&logMutex);

			//send results to the client
				//send the size of the data
				write(sfd, &retSize, sizeof(retSize));
				/*
				write(sfd, &argSize, sizeof(argSize));
				//send data
				for (i = 0; i < retSize; ++i) {
					for (j = 0; j < argSize; ++j) {
						//send the size of the string
						strSize = strlen(retVal[j][i]) + 1;
						write(sfd, &strSize, sizeof(strSize));
						//send string
						write(sfd, retVal[j][i], strSize);
					}
				}
				*/
				//free
				if (retVal != NULL) {
					for (i = 0; i < argSize; ++i) {
						if (retVal[i] != NULL) {
							for (j = 0; j < retSize; ++j)
								if (retVal[i][j] != NULL)
									free(retVal[i][j]);
							free(retVal[i]);
						}
					}
					free(retVal);
				}
				for (i = 0; i < argSize; ++i)
					free(args[i]);
				free(args);
			}
			//close connection
			close(sfd);
		}
		else 
			pthread_mutex_unlock(&queueMutex);
	}

	return NULL;
}


int main(int argc, char *argv[]) {
// signal handler	
	struct sigaction sa;
	sa.sa_handler = &sigHandler;
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) exit(1);

// vars
	int sfd, newsfd, port, poolSize,
		op, i;
	char *logfpath, *datasetfpath;
	FILE *datasetfp;
	struct sockaddr_in servaddr;
	char str[128], c[2];
	int quoteCount = 0;

// arg control
	while ((op = getopt(argc, argv, ":p:o:l:d:")) != -1){
		switch (op) {
			case 'p':
				port = atoi(optarg);
				break;
			case 'o':
				logfpath = optarg;
				break;
			case 'l':
				poolSize = atoi(optarg);
				break;
			case 'd':
				datasetfpath = optarg;
				break;
			case ':':
				fprintf(stderr, "Every command line argument option needs a value.\n");
				exit(1);
				break;
			case '?':
				fprintf(stderr, "Can only have parameters with -p, -o, -l, -d.\n");
				exit(1);
				break;
		}
	}
	//file open
	if ((datasetfp = fopen(datasetfpath, "r")) == NULL) exitprint(1, "fopen(dataset file)");

	if ((logfp = fopen(logfpath, "w")) == NULL) exitprint(1, "fopen(log file)");

	fprintf(logfp, "Executing with parameters:\n-p %d\n-o %s\n-l %d\n-d %s\n", port, logfpath, poolSize, datasetfpath);

// init socket
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) exitprint(1, "server socket");

	int opt = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if (bind(sfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
		fprintf(logfp, "%s\n", strerror(errno));
		exitprint(1, "server bind");
	}

	if (listen(sfd, 100) == -1) exitprint(1, "server listen");

// init monitors
	if (pthread_mutex_init(&queueMutex, NULL) != 0) exitprint(1, "monitor init");
	if (pthread_cond_init(&queueCond, NULL) != 0) exitprint(1, "monitor init");
	if (pthread_mutex_init(&dbMutex, NULL) != 0) exitprint(1, "monitor init");
	if (pthread_cond_init(&writeCond, NULL) != 0) exitprint(1, "monitor init");
	if (pthread_cond_init(&readCond, NULL) != 0) exitprint(1, "monitor init");
	if (pthread_mutex_init(&logMutex, NULL) != 0) exitprint(1, "monitor init");

// init database
	fprintf(logfp, "Loading dataset...\n");
	//fields
	int fieldCount = 0;
	int fieldCap = 10;
	char **fields = malloc(fieldCap * sizeof(char *));

	strcpy(str,"");
	do {
		c[0] = fgetc(datasetfp);
		//new field
		if (c[0] == ',') {
			//copy the string we have read to fields array
			fields[fieldCount] = malloc(strlen(str) * sizeof(char));
			strcpy(fields[fieldCount],str);
			//update field count
			fieldCount++;
			//reset str
			strcpy(str,"");
			//reallocate space if there is no more capacity
			if (fieldCount == fieldCap) {
				fieldCap += 5;
				fields = realloc(fields, fieldCap * sizeof(char *));
			}
		}
		//dont save quote char to the field name
		else if (c[0] == '"')
			quoteCount++;
		//control if char is not newline
		//if it is, control if it is inside a quotation
		else if ((c[0] != '\n' || quoteCount % 2 != 0) && c[0] != '\r' && c[0] != EOF)
			strcat(str, c);
		//newline char arrived indicating that first line is ended
		else {
			//copy the string we have read to fields array
			fields[fieldCount] = malloc(strlen(str) * sizeof(char *));
			strcpy(fields[fieldCount],str);
			//update field count
			fieldCount++;
			//finish reading
			break;
		}
		
	} while (!feof(datasetfp));

	//create
	db = malloc(sizeof(struct Database));
	createDatabase(fields,fieldCount);
	
	//fill db
	fillDatabase(datasetfp);
	fprintf(logfp, "Dataset loaded in ??? seconds with %d records.\n", db->size);
		
// init threads
	int id[poolSize];
	pthread_t threadPool[poolSize];

	fprintf(logfp, "A pool of %d threads has been created\n", poolSize);

	for (i = 0; i < poolSize; ++i) {
		id[i] = i;
		pthread_create(&threadPool[i], NULL, &poolRoutine, &id[i]);
	}

// client-server connect-accept
	while (!sigCtrl) {
		//accept new connection
		if ((newsfd = accept(sfd, (struct sockaddr*)NULL, NULL)) == -1 && errno != EINTR)
			exitprint(1, "server accept");
		//handle connection
		if (!sigCtrl) {
			if (pthread_mutex_lock(&queueMutex) != 0) exitprint(1, "MUTEX ERR");
			QueueAdd(&sfdQ, newsfd);
			taskCount++;
			if (pthread_mutex_unlock(&queueMutex) != 0) exitprint(1, "MUTEX ERR");
			pthread_cond_broadcast(&queueCond);
		}
	}

	pthread_mutex_lock(&logMutex);
	fprintf(logfp, "Termination signal received, waiting for ongoing threads to complete.\n");
	pthread_mutex_unlock(&logMutex);
	taskCount = 1;
	pthread_cond_broadcast(&queueCond);
	for (i = 0; i < poolSize; ++i)
		pthread_join(threadPool[i], NULL);
	fprintf(logfp, "All threads have terminated, server shutting down.\n");
	printf("All threads have terminated, server shutting down.\n");

	//close remaining sockets
	int soctemp;
	while ((soctemp = QueueRemove(&sfdQ)) != -1)
		close(sfd);

// free resources
	//files
	fclose(logfp);
	fclose(datasetfp);
	//vars
	cleanQueue(&sfdQ);
	//database
	removeDatabase();
	free(db);
	//monitors
	pthread_mutex_destroy(&queueMutex);
	pthread_mutex_destroy(&dbMutex);
	pthread_mutex_destroy(&logMutex);
	pthread_cond_destroy(&queueCond);
	pthread_cond_destroy(&writeCond);
	pthread_cond_destroy(&readCond);
	//socket
	close(sfd);

	return 0;
}