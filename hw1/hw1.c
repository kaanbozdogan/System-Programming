#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<dirent.h>
#include<string.h>
#include<sys/stat.h>
#include<signal.h>


struct searchData {
	// properties to llok for in a file
	char name[128];
	long int size;
	char type;
	char permissions[9];
	int linkCount;
	/*	boolean values of arguments.
		When the argument is given to the program the corresponding one becomes 1. */
	int b_name;
	int b_size;
	int b_type;
	int b_permissions;
	int b_linkCount;
};


void traverseDir(char *dirName, struct searchData sd, char **paths);
int isFileFound(char * path, struct dirent *dent, struct searchData sd);
int namecmp(char *str, char *name);
int isNumber(char *str);
int validStr(char *str, char *chars);
void printPath(char* path, int depth, int s, int e);
void printTree(char ** paths);

// Signal handler for ctrl+c signal.
void handler(int num) {
	printf("Ctrl+c signal recieved.\nExitting...\n");
	exit(1);
}

	
int main(int argc, char *argv[]) {
	signal(SIGINT, handler);
	
	int op, i;
	int control = 1;
	char *rootname;
	
	// information about the file we are searching
	struct searchData sd;
	sd.b_name = 0;
	sd.b_size = 0;
	sd.b_type = 0;
	sd.b_permissions = 0;
	sd.b_linkCount = 0;
	
	// given argumnt counts
	int arg_w = 0;
	int arg_other = 0;
	
	
	while ((op = getopt(argc, argv, "w:f:b:t:p:l:")) != -1){
		switch (op) {
			case 'w':
				rootname = optarg;
				arg_w ++;
				break;
			case 'f':
				// check the argument
				for(i = 0; (i < strlen(optarg)) && control == 1; i++) {
					if (optarg[i] == '/') {
						printf("-f parameter (searched file name) can not have slash (/) character in it.\n");
						control = 0;
					}
				}
				// update searching property
				strcpy(sd.name, optarg);
				sd.b_name = 1;
				arg_other ++;
				break;
			case 'b':
				// check argument
				if (isNumber(optarg) == 0) {
					printf("-b parameter must be a number.\n");
					control = 0;
				}
				// update searching property
				sd.size = atoi(optarg);
				sd.b_size = 1;
				arg_other ++;
				break;
			case 't':
				// check argument
				if ((strlen(optarg) > 1) || (validStr(optarg, "dsbcfpl") == 0)) {
					printf("-t parameter can only be one of 'd', 's', 'b', 'c', 'f', 'p' or 'l'.\n");
					control = 0;
				}
				// update searching property
				switch (optarg[0]) {
					case 'd':
						sd.type = DT_DIR;
						break;
					case 's':
						sd.type = DT_SOCK;
						break;
					case 'b':
						sd.type = DT_BLK;
						break;
					case 'c':
						sd.type = DT_CHR;
						break;
					case 'f':
						sd.type = DT_REG;
						break;
					case 'p':
						sd.type = DT_FIFO;
						break;
					case 'l':
						sd.type = DT_LNK;
						break;
				}
				sd.b_type = 1;
				arg_other ++;
				break;
			case 'p':
				// check argument
				if (strlen(optarg) != 9) {
					printf("-p parameter can only be 9 characters long.\n");
					control = 0;
				}
				if (validStr(optarg, "rwx-") == 0) {
					printf("-p parameter characters can only be one of 'r', 'w', 'x' or '-'\n");
					control = 0;
				}
				// update searching property
				strcpy(sd.permissions, optarg);
				sd.b_permissions = 1;
				arg_other ++;
				break;
			case 'l':
				// check argument
				if (isNumber(optarg) == 0) {
					printf("-b parameter must be a number.\n");
					control = 0;
				}
				// update searching property
				sd.linkCount = atoi(optarg);
				sd.b_linkCount = 1;
				arg_other ++;
				break;
			case '?':
				printf("Can only have parameters with -f, -b, -t, -p, -l.\n");
				control = 0;
				break;
		}
	}
	
	// check if -w argument is given
	if (arg_w == 0) {
		printf("Insufficent command line arguments. An argument with -w is required.\n");
		control = 0;
	}
	
	// check if at least one of the other arguments are given
	if (arg_other == 0) {
		printf("Insufficent command line arguments. An argument with at least one of -f, -b, -t, -p, -l is required.\n");
		control = 0;
	}
	
 	// check if arguments are valid
	if (control == 0)
		return 1;
	
	
	
	
	
	
	char **c = (char**) calloc(100,sizeof(char*));
	
	for(i = 0; i < 100; i++) {
		c[i] = malloc(256*sizeof(char));
		strcpy(c[i],"");
	}
	
	// search file with properites
	traverseDir(rootname, sd, c);
	printTree(c);
	
	// freeing
	for(i = 0; i < 100; i++)
		free(c[i]);
	free(c);
	
	
	return 0;
}



void traverseDir(char *path, struct searchData sd, char **paths) {
	DIR *dir;
	struct dirent *dent;
	int i;
	
	
	if ((dir = opendir(path)) != NULL) {
		while ((dent = readdir(dir)) != NULL) {
			// if curr file is not a link to itself or to its parent, continue.
			if (strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0) {
				// init path of the current directory
				char newPath[256];
				strcpy(newPath,"");
				strcat(newPath, path);
				if (strcmp(path, "/") != 0) strcat(newPath, "/");
				strcat(newPath, dent->d_name);
				// control if the file is found
				if(isFileFound(newPath, dent, sd) != 0) {	
					i = 0;
					while (strlen(paths[i]) > 0) i++;
					strcpy(paths[i],newPath);					
				}
				// if not continue recursion
				else if (dent->d_type == DT_DIR)
					traverseDir(newPath, sd, paths);
			}
		}
	}
	closedir(dir);
}


int isFileFound(char * path, struct dirent *dent, struct searchData sd) {
	struct stat filest;
	
	if(stat(path, &filest) < 0)
		return 0;
	
	// control if -f argument is given to the program
	if (sd.b_name == 1)
		// compare the name of the curr file and the searched name
		if (namecmp(dent->d_name, sd.name) == 0)
			return 0;
		
	if (sd.b_size == 1)
		if (filest.st_size != sd.size)
			return 0;
	
	if (sd.b_type == 1)
		if (dent->d_type != sd.type)
			return 0;
		
	if (sd.b_permissions == 1) {
		// Check the permissions one by one
		if (filest.st_mode & S_IRUSR) {
			if (sd.permissions[0] == '-') return 0;
		}else { 
			if (sd.permissions[0] == 'r') return 0;
		}
		if (filest.st_mode & S_IWUSR) {
			if (sd.permissions[1] == '-') return 0;
		}else { 
			if (sd.permissions[1] == 'w') return 0;
		}
		if (filest.st_mode & S_IXUSR) {
			if (sd.permissions[2] == '-') return 0;
		}else { 
			if (sd.permissions[2] == 'x') return 0;
		}
		if (filest.st_mode & S_IRGRP) {
			if (sd.permissions[3] == '-') return 0;
		}else { 
			if (sd.permissions[3] == 'r') return 0;
		}
		if (filest.st_mode & S_IWGRP) {
			if (sd.permissions[4] == '-') return 0;
		}else { 
			if (sd.permissions[4] == 'w') return 0;
		}
		if (filest.st_mode & S_IXGRP) {
			if (sd.permissions[5] == '-') return 0;
		}else { 
			if (sd.permissions[5] == 'x') return 0;
		}
		if (filest.st_mode & S_IROTH) {
			if (sd.permissions[6] == '-') return 0;
		}else { 
			if (sd.permissions[6] == 'r') return 0;
		}
		if (filest.st_mode & S_IWOTH) {
			if (sd.permissions[7] == '-') return 0;
		}else { 
			if (sd.permissions[7] == 'w') return 0;
		}
		if (filest.st_mode & S_IXOTH) {
			if (sd.permissions[8] == '-') return 0;
		}else { 
			if (sd.permissions[8] == 'x') return 0;
		}
	}
	
	if (sd.b_linkCount == 1)
		if (sd.linkCount != filest.st_nlink)
			return 0;
	
	
	return 1;
}


int namecmp(char *str, char *name) {
	int i, j;
	
	/* Iterate from the start of curr file's' name and searched name to their end */
	for(i = 0, j = 0; (i < strlen(str)) && (j < strlen(name)); i++, j++) {	
		// if curr char of the searched string is '+'
		if (name[j] == '+') {
			/* check until the char of the curr file's' name is not the same with the char before '+' at the searched name */
			while (str[i] == name[j-1] && i < strlen(str)) i ++;
			i --;
		}
		// compare the curr indexes
		else {
			if (str[i] != name[j])
				return 0;
		}
	}
	
	/* if the iteration of the strings are not finished at the same time, then the curr file is not a file we are searching */
	if (i != strlen(str) || j != strlen(name))
		return 0;
	else
		return 1;
}


int isNumber(char *str) {
	// control every char if they are a digit
	int i = 0;
	while (str[i] != '\0') {
		if (str[i] < '0' || str[i] > '9')
			return 0;
		i++;
	}
	return 1;
}

// control if str only consits of given chars
int validStr(char *str, char *chars) {
	int count;
	int i, j;
	
	// control every char at str
	for(i = 0; i < strlen(str); i++) {
		count = 0;
		/* control if the curr char of str is one of the chars at the given chars string */
		for(j = 0; j < strlen(chars); j++) {
			if (str[i] == chars[j])
				count ++;
		}
		// if curr char is not one of them
		if (count == 0)
			return 0;
	}
	return 1;
}


void printPath(char* path, int depth, int s, int e) {
	int i, j;
	
	if (path[s] == '/')
		printf("/");
	
	for(i = s; i < e; i++) {
		if (i == s)
			for(j = 0; j < depth; j++)
				printf("--");
			
		if (path[i] == '/') {
			depth ++;
			printf("\n");
			for(j = 0; j < depth; j++)
				printf("--");
		}
		else
			printf("%c", path[i]);
	}
	printf("\n");
}


void printTree(char **paths) {
	int i, j, l, lastSlash, depth, control;
	// calculate how many files found
	i = 0;
	while(strlen(paths[i]) > 0) i++;
	l = i;
	// print the first file found
	printPath(paths[0], 0, 0, strlen(paths[0]));
	/* calculate the common files between consecutive paths in the paths array.
	 * then only print the non-common files in order to give this printing a tree look*/
	for(i = 1; i < l; i++) {
		for(j = 0, lastSlash = 0, depth = 0, control = 1; j < strlen(paths[i-1]) && j < strlen(paths[i]) && control == 1; j++) {
			/* check if they have the same char at the same index
			 * if not stop checking because we found all of the common files between them */
			if (paths[i-1][j] != paths[i][j])
				control = 0;
			/* record the common slashes because after stopping the search for common files between them,
			 * we will print only after that slash */
			else if (paths[i-1][j] == '/' && paths[i][j] == '/') {
				lastSlash = j;
				depth ++;
			}
		}
		// printing the non-common part
		printPath(paths[i], depth, lastSlash+1, strlen(paths[i]));
	}
}














