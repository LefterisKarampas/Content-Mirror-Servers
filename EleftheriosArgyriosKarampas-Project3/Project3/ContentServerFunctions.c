#include <stdio.h>
#include <sys/wait.h>	     /* sockets */
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <unistd.h>	
#include <stdlib.h>	         
#include <string.h>
#include "ContentServerStruct.h"
#define LISTEN 20
#define N 20

int Create_Socket(int port){
	int sock;
	struct sockaddr_in server;
	struct sockaddr *serverptr=(struct sockaddr *)&server;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){				/* Create socket */
    	perror("socket");
    	exit(2);
    }
    server.sin_family = AF_INET;       								/* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);      							/* The given port */
    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0){
    	perror("bind");
    	exit(2);
    }
    /* Listen for connections */
    if (listen(sock, LISTEN) < 0){ 
    	perror("listen");
    	exit(2);
    }
    return sock;
}

int write_all(int fd, void *buff, size_t size) {
    int sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1) 
            return -1; /* error */
    }
    return sent;
}

unsigned long hash_function(char *str) // djb2 by Dan Bernstein 
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++) != '\0'){
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void initialize(HashTable *HT){
    *HT = malloc(sizeof(List *)*N);
    int i;
    for(i=0;i<N;i++){
        (*HT)[i] = NULL;
    }
}

/* Insert a new node to HashTable if it does not exists node with this key */
void HT_Insert(HashTable HT,Node *newNode){
    int pos = hash_function(newNode->key) % N;
    if(HT[pos] == NULL){                            /* Check if that bucket exists */
        HT[pos] = malloc(sizeof(List));             /* If not, create it */
        HT[pos]->x = newNode;                       /* Put the new node */
        HT[pos]->next = NULL;
    }
    else{
        int flag = 0;
        List *temp = HT[pos];
        if(!strcmp(temp->x->key,newNode->key)){
            temp->x->delay = newNode->delay;
            free(newNode->key);
            free(newNode);
            flag = 1;
        }
        while(!flag && temp->next != NULL){
            if(!strcmp(temp->x->key,newNode->key)){
                temp->x->delay = newNode->delay;
                free(newNode->key);
                free(newNode);
                flag = 1;
            }
            temp = temp->next;
        }
        if(!flag){
            temp->next = malloc(sizeof(List));
            temp = temp->next;
            temp->x = newNode;
            temp->next = NULL;
        }
    }
}

int HT_GetDelay(HashTable HT,char *key){
    int pos = hash_function(key) % N;                       /* Find bucket which can be the node with that key */
    if(HT[pos] == NULL){
        return 0;                                           
    }
    else{
        List *temp = HT[pos];
        while((strcmp(temp->x->key,key))){                  /* Search bucket */
            temp = temp->next;
            if(temp == NULL){
                return 0;
            }
        }
        return temp->x->delay;                              /* Return delay */
    }
}

void HT_Clear(HashTable *HT){
    int i;
    for(i=0;i<N;i++){
        if((*HT)[i] != NULL){
            List *current = (*HT)[i];
            List *temp;
            while(current){
                temp = current->next;
                free(current->x->key);
                free(current->x);
                free(current);
                current = temp;
            }
            
        }
    }
}