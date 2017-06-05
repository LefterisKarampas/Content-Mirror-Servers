#ifndef _CONTENTSERVER_H_
#define _CONTENTSERVER_H_

typedef struct node{
	char *key;
	int delay;
}Node;

typedef struct list{
	Node *x;
	struct list *next;
}List;

typedef List ** HashTable;

typedef struct info{
	int socket;
	char *ip;
}Info;

int Create_Socket(int);
int write_all(int , void *, size_t);
void initialize(HashTable *);
void HT_Insert(HashTable ,Node *);
int HT_GetDelay(HashTable ,char *);
void HT_Clear(HashTable *);
#endif 
