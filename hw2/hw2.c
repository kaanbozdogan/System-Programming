#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <math.h>

volatile sig_atomic_t sigcount1 = 0;
volatile sig_atomic_t child_exit_status;

void usr1handler(int sig) {
	sigcount1 = sigcount1 + 1;
}

void usr2handler(int sig) {
	/*intentionally empty*/
}

void sigchld_handler(int sig) {
	int status;
	wait(&status);
	child_exit_status = status;
}

double interpolate(double f[], double xi, int n);
int digit_count(double d);
int write_to_file(int fd, char* str);
double avg_err(int fd, int degree);
void lagrange_coef(double coefficients[], double xPoints[], double yPoints[], int len);


int main(int argc, char *argv[]) {
	struct sigaction sa1, sa_c, sigchld_a;
	sigset_t sigset1, sigset2, sigchldset;
	FILE *f;
	int fd, pids[8], i, j, control;
	double xy_i[16];
	char str[32], c[2];
	c[1] = '\0';
	
	// check command lien arg
	if (argc != 2) {
		printf("Program requires exactly one file as command line argument.\nExitting...\n");
		exit(1);
	}
	
	// open file
	if ((f = fopen(argv[1], "r+")) == NULL) {
		printf("Invalid file name. Can not open the file.\nExitting...\n");
		exit(1);
	}
	fd = fileno(f);

	// set sigusr1 handler for parent
	sigfillset(&sa1.sa_mask);
	sa1.sa_flags = SA_RESTART;
	sa1.sa_handler = usr1handler;
	if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
		fprintf(stderr, "parent sigaction for SIGUSR1\n");
		exit(1);
	}

	// set sig handler for SIGCHLD
	memset(&sigchld_a, 0, sizeof(sigchld_a));
	sigchld_a.sa_handler = &sigchld_handler;
	if (sigaction(SIGCHLD, &sigchld_a, NULL) == -1) {
		fprintf(stderr, "parent sigaction for SIGUSR2\n");
		exit(1);
	}

	// sig set for sigsuspend(sigusr1)
	sigfillset(&sigset1);
	sigdelset(&sigset1, SIGUSR1);
	sigdelset(&sigset1, SIGINT);
	// sig set for sigsuspend(sigusr2)
	sigfillset(&sigset2);
	sigdelset(&sigset2, SIGUSR2);
	sigdelset(&sigset2, SIGINT);
	// sig set for sigsuspend(sigchld)
	sigfillset(&sigchldset);
	sigdelset(&sigchldset, SIGCHLD);
	sigdelset(&sigchldset, SIGINT);

	// forking
	for(i = 0; i < 8; i++) {
		pids[i] = fork();
		if (pids[i] == -1) {
			fprintf(stderr, "fork for child no: %d\n", i);
			exit(1);
		}
		
	// child
		else if (pids[i] == 0) {
			sigfillset(&sa_c.sa_mask);
			sa_c.sa_flags = SA_RESTART;
			sa_c.sa_handler = usr2handler;
			if (sigaction(SIGUSR2, &sa_c, NULL) == -1) {
				fprintf(stderr, "child sigaction for SIGUSR2\n");
				exit(1);
			}

		// reading line
			if(flock(fd, LOCK_EX) == -1) {
				fprintf(stderr, "first flock for child: %d\n", i);
				exit(1);
			}
			strcpy(str, "");
			
			read(fd, c, 1);
			if (c[0] != '\n')
				lseek(fd, -1, SEEK_CUR);
			
			j = 0;
			do {
				read(fd, c, 1);
				if(c[0] != ',' && c[0] != '\n')
					strcat(str,c);
				else {
					xy_i[j] = atof(str);
					strcpy(str, "");
					j ++;
				}
			} while(c[0] != '\n');
			
		// calculating the degree 5 polynomial
			double res = interpolate(xy_i, xy_i[14], 6);
			strcpy(str, "");
			sprintf(str, ",%'.1lf", res);
			lseek(fd, -1, SEEK_CUR);
			int last_offset = write_to_file(fd, str);
			flock(fd, LOCK_UN);
			
		// signal parent
			if(kill(getppid(), SIGUSR1) == -1)
				fprintf(stderr, "kill(sigusr1) for child: %d\n", i);

		// wait for the signal from parent
			if (sigsuspend(&sigset2) == -1 && errno != EINTR)
				fprintf(stderr, "sigsuspend for sigset2 for child: %d\n", i);
			
		// calculating the degree 6
			if(flock(fd, LOCK_EX) == -1) {
				fprintf(stderr, "Second flock for child: %d\n", i);
				exit(1);
			}
			
			lseek(fd, last_offset, SEEK_SET);
			lseek(fd, -1, SEEK_CUR);
			do {
				control = read(fd, c, 1);
			} while(c[0] != '\n' && control != 0);
			lseek(fd, -1, SEEK_CUR);
			
			res = interpolate(xy_i, xy_i[14], 7);
			strcpy(str, "");
			sprintf(str, ",%'.1lf", res);
			write_to_file(fd, str);
			
		// calculate lagrange polynomial coefficients
			double coef[7], xs[7], ys[7];
			for(j = 0; j < 7; j++) {
				xs[j] = xy_i[2*j];
				ys[j] = xy_i[2*j + 1];
			}
			lagrange_coef(coef, xs, ys, 7);
			
		// calculate the curr line number
			int curr_offset = lseek(fd, 0, SEEK_CUR);
			last_offset = lseek(fd, 0, SEEK_END);
			lseek(fd, curr_offset, SEEK_SET);
			j = 0;
			do {
				read(fd, c, 1);
				if (c[0] == '\n')
					j++;
			} while (lseek(fd, 0, SEEK_CUR) < last_offset);
			lseek(fd, curr_offset, SEEK_SET);
		
		// print polinomial coefficients	
			printf("Polynomial %d: ", 8 - j);
			for(j = 0; j < 7; j++) {
				printf("%'.1lf", coef[j]);
				if (j < 6)
					printf(",");
			}
			printf("\n");
			
			flock(fd, LOCK_UN);

			exit(0);
		}
	}

	// parent
	if (pids[0] != 0 && pids[1] != 0 && pids[2] != 0 && pids[3] != 0 
		&& pids[4] != 0 && pids[5] != 0 && pids[6] != 0 && pids[7] != 0) {
		
		// wait for SIGUSR1 from children
		do {
			if(sigsuspend(&sigset1) == -1 && errno != EINTR)
				fprintf(stderr, "sigsuspend(sigset1) for parent\n");
		} while (sigcount1 < 8);
		
		printf("Error of polynomial of degree 5: %'.1lf\n", avg_err(fd, 5));

	// signal child that they can continue &  wait for SIGCHLD from children
		for(i = 0; i < 8; i++) {
			if(kill(pids[i], SIGUSR2) == -1)
				fprintf(stderr, "kill(sigusr2) for parent\n");
			
			if(sigsuspend(&sigchldset) == -1 && errno != EINTR)
				fprintf(stderr, "sigsuspend(sigchld) for parent\n");
		}

		printf("Error of polynomial of degree 6: %'.1lf\n", avg_err(fd, 6));

		fclose(f);
	}
	
	return 0;
}


double interpolate(double f[], double xi, int n) {
	double result = 0;
	int i, j;
	
	for (i=0; i<n; i++) {
		double term = f[2*i + 1];
		for (j=0; j<n; j++) {
			if (j != i) {
				term = (double)( term * (double)(xi - f[2*j]) ) /(double) ( f[2*i] - f[2*j] );
			}
		}
		result += term;
	}
	return result;
}

int digit_count(double d) {
	int digit = 0, num = d;
	
	do {
		digit++;
	} while ((num = num / 10) != 0);
	
	return digit;
}

int write_to_file(int fd, char* str) {
	char rest_of_file[1024];
	
	// saving the version before writing

	int curr_offset = lseek(fd, 0, SEEK_CUR);
	int last_offset = lseek(fd, 0, SEEK_END);
	int rem_chars = last_offset - curr_offset;
	lseek(fd, curr_offset, SEEK_SET);
	read(fd, rest_of_file, rem_chars);
	lseek(fd, curr_offset, SEEK_SET);

	// writing calculated data
	write(fd, str, strlen(str));
	curr_offset = lseek(fd, 0, SEEK_CUR);
	
	// restoring the rest of the file
	write(fd, rest_of_file, rem_chars);
	lseek(fd, curr_offset, SEEK_SET);
	return curr_offset;
}

double avg_err(int fd, int degree) {	
	double xy_i[16], err[8], total = 0;
	int i, j;
	char str[32], c[2];
	strcat(str, "");
	c[1] = '\0';
	
	// read file
	if(flock(fd, LOCK_EX) == -1) {
		printf("ERR: flock\n");
		exit(1);
	}
	lseek(fd, 0, SEEK_SET);
	// calc err for every line
	for(i = 0; i < 8; i++) {
		j = 0;
		do {
			read(fd, c, 1);
			if(c[0] != ',' && c[0] != '\n')
				strcat(str,c);
			else {
				xy_i[j] = atof(str);
				strcpy(str, "");
				j ++;
			}
		} while(c[0] != '\n');
		
		err[i] = xy_i[15] - xy_i[degree + 11];
	}
	flock(fd, LOCK_UN);
	
	for(i = 0; i < 8; i++) {
		total += fabs(err[i]);
	}
	return total / 8.0;
}

void lagrange_coef(double coefficients[], double xPoints[], double yPoints[], int len) {	
	int m, nc, startIndex, n;
	
	for (m = 0; m < len; m++) 
		coefficients[m] = 0;
	
	for (m = 0; m < len; m++) {
		double newCoefficients[len];
		for (nc = 0; nc < len; nc++) newCoefficients[nc]=0;
		
		if (m > 0) {
			newCoefficients[0] = -xPoints[0] / (xPoints[m] - xPoints[0]);
			newCoefficients[1] = 1 / (xPoints[m] - xPoints[0]);
		}
		else {
			newCoefficients[0] = -xPoints[1] / (xPoints[m] - xPoints[1]);
			newCoefficients[1] = 1 / (xPoints[m] - xPoints[1]);
		}
		
		startIndex = 1; 
		if (m == 0)
			startIndex = 2; 
		
		for (n = startIndex; n < len; n++) {
			if (m != n) {
				for (nc = len - 1; nc >= 1; nc--) 
					newCoefficients[nc] = newCoefficients[nc] * (-xPoints[n] / (xPoints[m] - xPoints[n])) + newCoefficients[nc - 1] / (xPoints[m] - xPoints[n]);
				newCoefficients[0] = newCoefficients[0] * (-xPoints[n] / (xPoints[m] - xPoints[n]));
			}
		}	
		for (nc = 0; nc < len; nc++)
			coefficients[nc] += yPoints[m] * newCoefficients[nc];
	}
}



