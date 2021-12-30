#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#include "DataStructures.h"


// sig handler
sig_atomic_t sigCtrl = 0;

void sigHandler(int sigNum) {
	sigCtrl = 1;
}

//semaphores
sem_t sem_q; //for queue access
sem_t hwCount; //for hw count in the queue
sem_t avlStdCount; //for student count who is not busy doing work
sem_t print; //for synchronization of prints
//pipe
int **p_stds; //to senf hws to the students from main thread
// global vars
FILE *hwfp;
int budget;


// threads
void * g_routine(void *arg) {
	struct Queue *hws = (struct Queue *) arg;
	char h;
	//read homeworks
	fscanf(hwfp, "%c", &h);
	while (!feof(hwfp) && !sigCtrl && budget > 0) {
		if (sem_wait(&print) == -1) exitprint(1, "g_rouine: sem_wait(print)");
		printf("G has a new homework %c; remainig money is %dTL\n", h, budget);
		if (sem_post(&print) == -1) exitprint(1, "g_rouine: sem_post(print)");
		//add to queue
		if (sem_wait(&sem_q) == -1) exitprint(1, "g_rouine: sem_wait(sem_q)");
		QueueAdd(hws, h);
		if (sem_post(&sem_q) == -1) exitprint(1, "g_rouine: sem_post(sem_q)");
		if (sem_post(&hwCount) == -1) exitprint(1, "g_rouine: sem_post(hwCount)");
		//read from file
		fscanf(hwfp, "%c", &h);
	}
	//to prevent a possible deadlock between g and main thread
	if (sigCtrl) {
		QueueAdd(hws,-1);
		if (sem_post(&hwCount) == -1) exitprint(1, "g_rouine: sem_post(hwCount)");
	}

	return NULL;
}

void * std_routine(void *arg) {
	struct Student *stats = (struct Student *) arg;
	char hw;

	while (!sigCtrl) {
		//wait for new assignment
		printf("%s is waiting for a homework\n", stats->name);
		if (read(p_stds[stats->index][0], &hw, sizeof(hw)) == -1) exitprint(1, "std_routine: read");
		//is there any homework left to do
		if (hw != -1) {
			printf("%s is solving homework %c for %d, G has %dTL left.\n", stats->name, hw, stats->price, budget);
			stats->solved++;
			if (sem_post(&print) == -1) exitprint(1, "std_routine: sem_post(print)");
			sleep(6 - stats->speed);
			
			stats->busy = 0;
			if (sem_post(&avlStdCount) == -1) exitprint(1, "std_routine: sem_post(avlStdCount)");
		}
		else
			break;
	}

	return NULL;
}


int main(int argc, char const *argv[]) {
//set sig handler for SIGINT
	struct sigaction sa;
	sa.sa_handler = &sigHandler;
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) == -1) exitprint(1, "main: sigaction");

	FILE *sfp;
	int i, j, sfd, sCount;
	char c;
	pthread_t thread_g, *thread_stds;
	struct Queue *hws;
	struct Student *stds;
	int *sortIndex[3];
	int *currSort;
	char curr_hw;

// arg control
	if (argc != 4) {
		fprintf(stderr, "Usage: ./hw4 homeworkFilePath thread_stdsFilePath budget\n");
		exit(1);
	}
	if ((hwfp = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "Can not open %s. Error: %s\n", argv[1], strerror(errno));
		exit(1);
	}
	if ((sfp = fopen(argv[2], "r")) == NULL) {
		fprintf(stderr, "Can not open %s. Error: %s\n", argv[2], strerror(errno));
		exit(1);
	}
	if ((budget = atoi(argv[3])) < 0) {
		fprintf(stderr, "Budget can not be smaller than 0\n");
		exit(1);
	}

// read students
	//calculate std count
	sfd = fileno(sfp);
	sCount = 0;
	while (read(sfd, &c, 1) != 0)
		if (c == '\n') sCount++;
	//if there is no students in the file
	if (sCount == 0) {
		fprintf(stderr, "Students file has no students in it\n");
		exit(1);
	}
	//the last line in the file has no \n character
	else
		sCount++;
	//create student array with the student count
	if ((stds = (struct Student *) malloc(sCount * sizeof(struct Student))) == NULL) exitprint(1, "main: malloc 1");
	//return to the begining of the file
	lseek(sfd, 0, SEEK_SET);

	for (i = 0; i < sCount; ++i) {
		fscanf(sfp, "%s %d %d %d\n", stds[i].name, &stds[i].quality, &stds[i].speed, &stds[i].price);
		stds[i].busy = 0;
		stds[i].solved = 0;
		stds[i].index = i; //used to send message to the students pipe and finding it in students array when needed
	}

// sort students acording to C Q S
	//create arrays
	for (i = 0; i < 3; ++i)
		if ((sortIndex[i] = (int *) malloc(sCount * sizeof(int))) == NULL) exitprint(1, "main: malloc 2");
	//this array has the indexes of the students acording to the corresponding priorities
	//[0]: C, [1]: Q, [2]: S
	for (i = 0; i < 3; ++i)
		for (j = 0; j < sCount; ++j)
			sortIndex[i][j] = j; //give the indexes in orginal order
	//sort
	sort(stds, sortIndex[0], sCount, 'C');
	sort(stds, sortIndex[1], sCount, 'Q');
	sort(stds, sortIndex[2], sCount, 'S');

// init semaphores and pipe
	//sem
	if (sem_init(&sem_q, 0, 1) == -1) exitprint(1, "main: sem_init(sem_q)");
	if (sem_init(&hwCount, 0, 0) == -1) exitprint(1, "main: sem_init(hwCount)");
	if (sem_init(&avlStdCount, 0, sCount) == -1) exitprint(1, "main: sem_init(avlStdCount)");
	if (sem_init(&print, 0, 1) == -1) exitprint(1, "main: sem_init(print)");
	//pipes
	if ((p_stds = (int **) malloc(sCount * sizeof(int *))) == NULL) exitprint(1, "main: malloc 3");
	for (i = 0; i < sCount; ++i) {
		if ((p_stds[i] = (int *) malloc(2 * sizeof(int))) == NULL) exitprint(1, "main: malloc 4");
		if (pipe(p_stds[i]) == -1) exitprint(1, "main: pipe()");
	}
	
// create queue and treads
	if ((hws = (struct Queue *) malloc(sizeof(struct Queue *))) == NULL) exitprint(1, "main: malloc 5");
	//g thread
	if (pthread_create(&thread_g, NULL, g_routine, hws) == -1) exitprint(1, "pthread_create(g)");
	//std threads
	if (sem_wait(&print) == -1) exitprint(1, "main: sem_wait(print) 1");
	//print students
	printf("%d students-for-hire threads have been created.\n", sCount);
	printf("Name  Q S C\n");
	for (i = 0; i < sCount; ++i)
		printf("%s %d %d %d\n", stds[i].name, stds[i].quality, stds[i].speed, stds[i].price);
	if (sem_post(&print) == -1) exitprint(1, "main: sem_post(print)");
	//create students
	if ((thread_stds = malloc(sCount * sizeof(pthread_t))) == NULL) exitprint(1, "main: malloc 6");
	for (i = 0; i < sCount; ++i)
		if (pthread_create(&thread_stds[i], NULL, &std_routine, &stds[i]) == -1) exitprint(1, "pthread_create(student)");

// do hws
	while (budget > 0 && (!QueueIsEmpty(hws) || !feof(hwfp)) && !sigCtrl) {
		//wait for new hw
		if (sem_wait(&hwCount) == -1) exitprint(1, "main: sem_wait(hwCount)");
		if (sem_wait(&sem_q) == -1) exitprint(1, "main: sem_wait(sem_q)");
		if (sem_wait(&print) == -1) exitprint(1, "main: sem_wait(print) 2");
		//read from queue
		curr_hw = QueueRemove(hws);
		if (sem_post(&sem_q) == -1) exitprint(1, "main: sem_post(sem_q)");
		//wait until at least one student is available
		if (sem_wait(&avlStdCount) == -1) exitprint(1, "main: sem_wait(avlStdCount) 1");
		//determine which array is going to be used acording to hw type
		switch (curr_hw) {
			case 'C':
				currSort = sortIndex[0];
				break;
			case 'Q':
				currSort = sortIndex[1];
				break;
			case 'S':
				currSort = sortIndex[2];
				break;
		}
		//assign hw to student acording to best fit
		if (!sigCtrl) {
			for (i = 0; i < sCount; ++i){
				//if std is busy or budget is not enough for it, try the next best fit
				if (stds[currSort[i]].busy == 0 && stds[currSort[i]].price <= budget) {
					stds[currSort[i]].busy = 1;
					budget -= stds[currSort[i]].price;
					//send hw through pipe
					if (write(p_stds[currSort[i]][1], &curr_hw, sizeof(curr_hw)) == -1) exitprint(1, "main: write 1");
					break;
				}
			}
		}
		//if remaining budget is not enough for any available student, shutdown
		if (i == sCount) {
			pthread_cancel(thread_g);
			printf("G has no more money for homeworks, terminating.\n");
			//couldn't give hw to any student in this loop,
			//so decremented avlStdCount at the begining of the while loop is incremented.
			if (sem_post(&avlStdCount) == -1) exitprint(1, "main: sem_post(avlStdCount)");
			printf("Money is over, closing.\n");
			break;
		}
		
	}

//finishing threads
	//if SIGINT has not arrived wait for threads to finish
	if (!sigCtrl) {
		if (i != sCount)  //program finished because there is no more hws
			printf("No more homeworks left or coming in, closing.\n");
		//wait until everybody finishes their work
		for (i = 0; i < sCount; ++i)
			if (sem_wait(&avlStdCount) == -1) exitprint(1, "main: sem_wait(avlStdCount) 2");
		//make sure threads which are locked waiting for new hw are notified to finish
		char termCond = -1;
		for (i = 0; i < sCount; ++i)
			if (write(p_stds[i][1], &termCond, sizeof(termCond)) == -1) exitprint(1, "main: write 2");
	}
	//SIGINT arrived terminate threads
	else {
		printf("\nTermination signal recieved, closing.\n");
		pthread_cancel(thread_g);
		for (i = 0; i < sCount; ++i)
			pthread_cancel(thread_stds[i]);
	}

//money report
	int totalCount = 0;
	int totalCost = 0;
	printf("Homeworks solved and money made by students:\n");
	for (i = 0; i < sCount; ++i) {
		totalCount += stds[i].solved;
		totalCost += stds[i].solved * stds[i].price;
		printf("%s %d %d\n", stds[i].name, stds[i].solved, stds[i].solved * stds[i].price);
	}
	printf("Total cost for %d homeworks %dTL\n", totalCount, totalCost);
	printf("Money left at G's accaunt: %dTL\n", budget);

// free resources
	//files
	fclose(hwfp);
	fclose(sfp);
	//threads
	pthread_detach(thread_g);
	for (i = 0; i < sCount; ++i)
		pthread_join(thread_stds[i], NULL);
	free(thread_stds);
	//semaphores
	sem_destroy(&sem_q);
	sem_destroy(&hwCount);
	sem_destroy(&avlStdCount);
	//pipe
	for (i = 0; i < sCount; ++i)
		free(p_stds[i]);
	free(p_stds);
	//data structures
	cleanQueue(hws);
	free(hws);
	free(stds);
	for (i = 0; i < 3; ++i)
		free(sortIndex[i]);

	return 0;
}