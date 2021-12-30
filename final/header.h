#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//HELPER FUNCTIONS
void exitprint(int status, char* output) {
	fprintf(stderr, "%s\n", output);
	exit(status);
}

//controls if the given string includes the given char
//if it is return the count of it
int strinc(char *str, char c) {
	int i, count = 0;;

	if (str != NULL) {		
		for (i = 0; i < strlen(str); ++i)
			if (str[i] == c)
				count++;
		return count;
	}
	else
		return -1;
}


//QUEUE
struct Queue {
	struct QueueNode *head;
	int size;
};

struct QueueNode {
	int data;
	struct QueueNode *next;
};

void QueueAdd(struct Queue *q, int newQuery) {
	struct QueueNode *curr;

	if (q->head != NULL) {
		curr = q->head;
		while(curr->next != NULL)
			curr = curr->next;

		curr->next = malloc(sizeof(struct QueueNode *));
		curr->next->data = newQuery;
		q->size++;
	}
	else {
		q->head = malloc(sizeof(struct QueueNode *));
		q->head->data = newQuery;
		q->size = 1;
	}
}

int QueueRemove(struct Queue *q) {
	struct QueueNode *curr = q->head;
	int retVal;

	if (curr != NULL) {
		retVal = curr->data;
		q->head = curr->next;
		free(curr);
		q->size--;
	}
	else
		retVal = -1;

	return retVal;
}

int QueueIsEmpty(struct Queue *q) {
	return q->head == NULL;
}

void cleanQueue(struct Queue *q) {
	struct QueueNode *curr = q->head;
	struct QueueNode *temp;
	while (q->size > 0 && curr != NULL) {
		temp = curr;
		curr = curr->next;
		free(temp);
		q->size--;
	}
}


#endif