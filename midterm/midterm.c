#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>




int main(int argc, char *argv[]) {
	int op, optargCount = 0, i;
	int nurCount, 
		vacCount, 
		citCount,
		bufSize,
		shotCount;
	char fileName[128];
	FILE *fp;
	char str[256];


	while ((op = getopt(argc, argv, ":n:v:c:b:t:i:")) != -1){
		switch (op) {
			case 'n':
				if ((nurCount = atoi(optarg)) < 2)
					exit(1);
				optargCount ++;
				break;
			case 'v':
				if ((vacCount = atoi(optarg)) < 2)
					exit(1);
				optargCount ++;
				break;
			case 'c':
				if ((citCount = atoi(optarg)) < 3)
					exit(1);
				optargCount ++;
				break;
			case 't':
				if ((shotCount = atoi(optarg)) < 1)
					exit(1);
				optargCount ++;
				break;
			case 'b':
				bufSize = atoi(optarg);
				optargCount ++;
				break;
			case 'i':
				strcpy(fileName, optarg);
				optargCount ++;
				break;
			case ':':
				printf("Every command line argument option needs a value.\n");
				break;
			case '?':
				printf("Can only have parameters with -n, -v, -c, -b, -t, -i.\n");
				exit(1);
				break;
		}
	}

	if (optargCount != 6) {
		printf("%d Not enough command line arguments.\n", optargCount);
		exit(1);
	}
	else if(bufSize < (citCount * shotCount + 1)) {
		printf("Buffer size (%d) is samaller than c*t+1\n", bufSize);
		exit(1);
	}
	else
		printf("Welcome to the GTU344 clinic. Number of citizens to vaccinate c=%d with t=%d doses.\n", citCount, shotCount);

	// open file
	if ((fp = fopen(fileName, "r")) == NULL) {
		printf("file open error. %s\n", strerror(errno));
		exit(1);
	}
	int input_fd = fileno(fp);
	
// semaphore init
	sem_t *critSec = sem_open("critSec", O_CREAT, 0600, 1);
	sem_t *empty = sem_open("empty", O_CREAT, 0600, bufSize);
	sem_t *vac1 = sem_open("vac1", O_CREAT, 0600, 0);
	sem_t *vac2 = sem_open("vac2", O_CREAT, 0600, 0);
	sem_t *getVac = sem_open("getVac", O_CREAT, 0600, 0);
	sem_t *citLeave = sem_open("citLeave", O_CREAT, 0600, 0);
	sem_t *nursesDone = sem_open("nursesDone", O_CREAT, 0600, 0);

// shared memory init
	// buffer
	int mem_bufSize = bufSize;  //size of allocated shared memory
	int buf_fd = shm_open("/buffer", O_CREAT | O_RDWR, S_IRWXU);
	if (buf_fd == -1) {
		fprintf(stderr, "shm_open %s\n", strerror(errno));
		exit(1);
	}

	if (ftruncate(buf_fd, mem_bufSize) == -1) {
		fprintf(stderr, "ftruncate %s\n", strerror(errno));
		exit(1);
	}
	
	char *buf = (char*) mmap(NULL, mem_bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, buf_fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "mmap\n");
		exit(1);
	}

	// information array
	int infArrSize = vacCount + 3;
	int inf_fd = shm_open("/inf", O_CREAT | O_RDWR, S_IRWXU);
	if (inf_fd == -1) {
		fprintf(stderr, "shm_open %s\n", strerror(errno));
		exit(1);
	}

	if (ftruncate(inf_fd, infArrSize*sizeof(int *)) == -1) {
		fprintf(stderr, "ftruncate %s\n", strerror(errno));
		exit(1);
	}
	
	int *inf = (int*) mmap(NULL, infArrSize, PROT_READ | PROT_WRITE, MAP_SHARED, inf_fd, 0);
	if (inf == MAP_FAILED) {
		fprintf(stderr, "mmap\n");
		exit(1);
	}

	// vaccinator's vaccination count in the first vacCount indexes
	for (i = 0; i < infArrSize; ++i) {
		inf[i] = 0;
	}
	int bufIndex = vacCount,  //first empty index of the buffer
		i1 = vacCount + 1,  //vac1 count index of the buffer
		i2 = vacCount + 2;  //vac2 count index of the buffer


// fork
	int n = nurCount + vacCount + citCount;
	int childs[n];
	for (i = 0; i < n; ++i) {
		childs[i] = fork();

		if (childs[i] == -1) {
			fprintf(stderr, "fork child %d\n", i);
			exit(1);
		}
		else if (childs[i] == 0) {

		// nurse
			if (i < nurCount) {
				char input_c;

				while(1) {
					// control if there is any vaccine remaining in the input file
					if (lseek(input_fd, 0, SEEK_CUR) >= shotCount * citCount * 2) {
            			sem_post(nursesDone);
            			break;
            		}

					sem_wait(empty); //wait until there is emty space in buffer
					sem_wait(critSec); //wait for critical section

					// control if there is any vaccine remaining in the input file
					if (lseek(input_fd, 0, SEEK_CUR) >= shotCount * citCount * 2) {
            			sem_post(nursesDone);
            			sem_post(critSec);
            			break;
            		}
					
					// get vaccine from the input file and put it into buffer
					read(input_fd, &input_c, 1);
					buf[inf[bufIndex]] = input_c;
					inf[bufIndex]++;

					// increase the semaphore of the corresponding vaccine
					if (input_c == '1') {
						inf[i1] ++;
						sem_post(vac1);
					}
					else {
						inf[i2] ++;
						sem_post(vac2);
					}

					sprintf(str, "Nurse %d (pid=%d) has brought vaccine %c: the clinic has %d vaccine1 and %d vaccine2.\n", i+1, getpid(), input_c, inf[i1], inf[i2]);
					write(STDOUT_FILENO, str, strlen(str));
					fflush(stdout);
					
					sem_post(critSec); //release critical section
				}

			// free resources
				sem_close(critSec);
				sem_close(empty);
				sem_close(vac1);
				sem_close(vac2);
				sem_close(getVac);
				sem_close(citLeave);
				sem_close(nursesDone);
				munmap(buf, mem_bufSize);
				munmap(inf, infArrSize * sizeof(int *));

				exit(0);
			}

		// vaccinator
			else if (i < vacCount + nurCount) {
				//printf("vaccinator %d\n", getpid());
				int v1, v2, smaller, bigger, j;
				
				while(1) {

					sem_wait(vac1); //wait until there is enough vaccine 1
					sem_wait(vac2); //wait until there is enough vaccine 2
					sem_wait(critSec); //wait for crtitical section

					// increment the vaccination count for the vaccinator
					inf[i-nurCount]++;
					// decrement the vaccine count in the clinic
					inf[i1]--;
					inf[i2]--;
					
					// buffer work
					for (j = 0, v1 = -1, v2 = -1; j < inf[bufIndex]; ++j) {
						if (buf[j] == '1')
							v1 = j;
						else
							v2 = j;
						// found first and second vaccine. stop searching
						if (v1 != -1 && v2 != -1)
							break;
					}

					// compare the indexes of vaccines
					if (v1 <= v2) { smaller = v1; bigger = v2; }
					else { smaller = v2; bigger = v1; }

					// remove vaccines from buffer and shift the ramaining values
					for (j = smaller + 1; j < bigger; ++j)
						buf[j - 1] = buf[j];
					for (j = bigger + 1; j < inf[bufIndex]; ++j)
						buf[j - 2] = buf[j];
					// update the vaccine count
					inf[bufIndex] -= 2;

					sprintf(str, "Vaccinator %d (pid=%d) is inviting citizen ", i-nurCount+1, getpid());
					write(STDOUT_FILENO, str, strlen(str));
					fflush(stdout);

					sem_post(getVac); //one shot of vaccination is ready for citizens
					sem_wait(citLeave); //wait for the citizen to leave
					sem_post(critSec); //release critical section 
					sem_post(empty); //two empty space is created
					sem_post(empty);
				}

			// free resources
				sem_close(critSec);
				sem_close(empty);
				sem_close(vac1);
				sem_close(vac2);
				sem_close(getVac);
				sem_close(citLeave);
				sem_close(nursesDone);
				munmap(buf, mem_bufSize);
				munmap(inf, infArrSize * sizeof(int *));
				
				exit(0);
			}

		// citizen
			else {
				//printf("citizens %d\n", getpid());
				int j;
				for (j = 0; j < shotCount; ++j){
					sem_wait(getVac); //wait until one shot of the vaccine is ready

					sprintf(str, "pid=%d to the clinic.\n", getpid());
					write(STDOUT_FILENO, str, strlen(str));
					fflush(stdout);

					sem_post(citLeave);

					sprintf(str, "Citizen %d (pid=%d) is vaccinated for the %dth time: the clinic has %d vaccine1 and %d vaccine2\n", i-nurCount-vacCount+1, getpid(), j+1, inf[i1], inf[i2]);
					write(STDOUT_FILENO, str, strlen(str));
					fflush(stdout);
				}
				
			// free resources
				sem_close(critSec);
				sem_close(empty);
				sem_close(vac1);
				sem_close(vac2);
				sem_close(getVac);
				sem_close(citLeave);
				sem_close(nursesDone);
				munmap(buf, mem_bufSize);
				munmap(inf, infArrSize * sizeof(int *));
				
				exit(0);
			}
		}
	}
	
// parent
	sem_wait(nursesDone);
	sem_wait(critSec);
	printf("Nurses have carried all vaccines to the buffer, terminating.\n");
	fflush(stdout);
	sem_post(critSec);

	waitpid(-1, NULL, 0);

	printf("All citizens have been vaccinated .\n");
	fflush(stdout);
	for (i = 0; i < vacCount; ++i) {
		printf("Vaccinator %d (pid=%d) vaccinated %d doses. ", i+1, getpid(), inf[i]);
		fflush(stdout);
	}
	printf("The clinic is now closed. Stay healthy.\n");
	fflush(stdout);

// free resources
	sem_close(critSec);
	sem_close(empty);
	sem_close(vac1);
	sem_close(vac2);
	sem_close(getVac);
	sem_close(citLeave);
	sem_close(nursesDone);
	sem_unlink("critSec");
	sem_unlink("empty");
	sem_unlink("vac1");
	sem_unlink("vac2");
	sem_unlink("getVac");
	sem_unlink("citLeave");
	sem_unlink("nursesDone");

	munmap(buf, mem_bufSize);
	munmap(inf, infArrSize * sizeof(int *));
	shm_unlink("/buffer");
	shm_unlink("/inf");

	return 0;
}