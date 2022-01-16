#ifndef DATABASE_H
#define DATABASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"

struct Database *db;

struct Database {
	char ***data; //[col][row][string]
	char **fields;
	int fieldSize;
	int size;
	int capacity;
};


void createDatabase(char **newFields, int newFieldSize) {
	int i;
	// fields	
	//create fields
	db->fieldSize = newFieldSize;
	db->fields = malloc(db->fieldSize * sizeof(char *));
	//init fields
	for (i = 0; i < db->fieldSize; ++i) {
		db->fields[i] = malloc(strlen(newFields[i]) * sizeof(char));
		strcpy(db->fields[i], newFields[i]);
	}
	// data
	db->size = 0;
	db->capacity = 300;
	//allocate space
	db->data = malloc(db->fieldSize * sizeof(char **));
	for (i = 0; i < db->fieldSize; ++i)
		db->data[i] = malloc(db->capacity * sizeof(char *));

	//free newFields
	for (i = 0; i < newFieldSize; ++i)
		free(newFields[i]);
	free(newFields);
}


void addRecord(char record[][128]) {
	int i;

	//if database is full
	if (db->size == db->capacity) {
		db->capacity += 300;
		for (i = 0; i < db->fieldSize; ++i)
			db->data[i] = realloc(db->data[i], db->capacity * sizeof(char *));
	}
	//add record to the end
	for (i = 0; i < db->fieldSize; ++i) {
		db->data[i][db->size] = malloc(strlen(record[i]) * sizeof(char));
		strcpy(db->data[i][db->size], record[i]);
	}
	db->size++;
}


void fillDatabase(FILE *fp) {
	int i = 0, j = 0, quoteCount = 0;
	char record[db->fieldSize][128], str[128], c[2] = "_";
	strcpy(str,"");

	do {
		c[0] = fgetc(fp);
		//new field of object
		if (c[0] == ',') {
			//copy the string we have read to fields array
			strcpy(record[i],str);
			//update field index
			i++;
			//reset str
			strcpy(str,"");
		}
		//dont save quote char to the field name
		else if (c[0] == '"')
			quoteCount++;
		//control if char is not newline or end of file
		//if it is new line, control if it is inside a quotation
		else if ((c[0] != '\n' || quoteCount % 2 != 0) && c[0] != '\r' && c[0] != EOF)
			strcat(str, c);
		//newline or eof char arrived
		else {
			//if there is eof rigth after new line charater
			if (strlen(str) > 0) {
				//copy the string we have read to fields array
				strcpy(record[i],str);
				//update field index
				i++;
				//add record of the line to the db
				addRecord(record);
			}
			//reset field index and strings if it is not eof
			if (!feof(fp)) {	
				i = 0;
				strcpy(str,"");
				for (j = 0; j < db->fieldSize; ++j)
					strcpy(record[j],"");
			}
		}
	} while (!feof(fp));
}


char ** parseQuery(char query[], char *command, int *tokSize) {
	char *token, *saveptr, delim[7];
	int i, j, k, quoteCount = 0, tokCap = 10;
	strcpy(delim, " ,;=\n\t");
	char **tokens = malloc(tokCap * sizeof(char *));
	int size = 0;

	/* tokenize	*/
	//parse the whole query into tokens
	token = strtok_r(query, delim, &saveptr);
	while (token != NULL) {
		//if there is no open quotes
		if (quoteCount % 2 == 0) {
			//realloc if size of the array is not enough
			if (size == tokCap) {
				tokCap += 5;
				tokens = realloc(tokens, tokCap * sizeof(char *));
			}
			//alloc
			tokens[size] = malloc(strlen(token) * sizeof(*token));
			//add token to array
			strcpy(tokens[size], token);
			size++;
		}
		//if there is an open quote from previous token, that token still continues
		//strcat this token to that token
		else {
			//realloc
			tokens[size-1] = realloc(tokens[size-1],
				(strlen(tokens[size-1]) + strlen(token) + 1) * sizeof(char));
			//append
			strcat(tokens[size-1], " ");
			strcat(tokens[size-1], token);
		}
		quoteCount += strinc(token, '\'');
		//continue parsing
		token = strtok_r(NULL, delim, &saveptr);
	}
	/* init arguments of the given query */
	//SELECT / SELECT DISTINCT
	if (strcmp(tokens[0], "SELECT") == 0) {
		//determine if the query is SELECT DISTINCT or not
		if (strcmp(tokens[1], "DISTINCT") == 0) {
			*command = 'D';
			j = 2;
		}
		else {
			*command = 'S';
			j = 1;
		}
		//remove "SELECT", "DISTINCT", "FROM TABLE" from tokens array
		for (i = j; i < size; ++i)
			strcpy(tokens[i-j], tokens[i]);
		//free the unused strings
		for (i = size - (j + 2); i < size; ++i)
			free(tokens[i]);
		//update the token count
		size -= (j + 2);
	}
	//UPDATE
	else {
		*command = 'U';
		//remove "UPDATE TABLE SET" part of the query from tokens
		for (i = 3; i < size - 3; ++i)
			strcpy(tokens[i-3], tokens[i]);
		//remove  "WHERE" part of the query from tokens
		for (i = size-2; i < size; ++i)
			strcpy(tokens[i-4], tokens[i]);
		//free the unused strings
		for (i = size - 4; i < size; ++i)
			free(tokens[i]);
		//update the token count
		size -= 4;
		//remove quote chars from arguments
		for (i = 1; i < size; i += 2) {
			k = strlen(tokens[i]);
			for (j = 1; j < k-1; ++j)
				tokens[i][j-1] = tokens[i][j];
			tokens[i][k-2] = '\0';
		}
	}

	*tokSize = size;
	return tokens;
}


void removeDatabase() {
	int i, j;
	//fields
	if(db->fields != NULL) {
		for (i = 0; i < db->fieldSize; ++i) {
			if(db->fields[i] != NULL) {
				free(db->fields[i]);
			}
		}
		free(db->fields);
	}
	//data
	if (db->data != NULL) {
		for (i = 0; i < db->fieldSize; ++i) {
			if (db->data[i] != NULL) {
				for (j = 0; j < db->capacity; ++j) {
					if (db->data[i][j] != NULL) {
						free(db->data[i][j]);
					}
				}
				free(db->data[i]);
			}
		}
		free(db->data);
	}
}


char *** databaseSelect(char **args, int *argSize, int *retSize);


char *** databaseSelectDist(char **args, int *argSize, int *retSize);


char *** databaseUpdate(char **args, int *argSize, int *retSize);


#endif
