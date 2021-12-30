#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <stdio.h>
#include <stdlib.h>


struct Student {
	char name[64];
	int quality;
	int speed;
	int price;
	int busy;
	int solved;
	int index;
};


struct Queue {
	struct QueueNode *head;
	int size;
};

struct QueueNode {
	char data;
	struct QueueNode *next;
};

void QueueAdd(struct Queue *q, char c) {
	struct QueueNode *curr;

	if (q->head != NULL) {
		curr = q->head;
		while(curr->next != NULL)
			curr = curr->next;

		curr->next = malloc(sizeof(struct QueueNode *));
		curr->next->data = c;
		q->size++;
	}
	else {
		q->head = malloc(sizeof(struct QueueNode *));
		q->head->data = c;
		q->size = 1;
	}
}

char QueueRemove(struct Queue *q) {
	struct QueueNode *curr = q->head;
	char retVal;

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


void swap(int *a, int *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

void sort(struct Student stds[], int sorted[], int sCount, char param) {
	int i, j;

	for (i = 0; i < sCount - 1; ++i) {
		for (j = 0; j < sCount - i - 1; ++j) {
			switch (param) {
				case 'C':
					if (stds[sorted[j]].price > stds[sorted[j + 1]].price) {
						swap(&sorted[j], &sorted[j + 1]);
					}
					break;
				case 'Q':
					if (stds[sorted[j]].quality < stds[sorted[j + 1]].quality)
						swap(&sorted[j], &sorted[j + 1]);
					break;
				case 'S':
					if (stds[sorted[j]].speed < stds[sorted[j + 1]].speed)
						swap(&sorted[j], &sorted[j + 1]);
					break;
			}
		}
	}
}


void exitprint(int status, char* output) {
	fprintf(stderr, "%s\n", output);
	exit(status);
}


#endif