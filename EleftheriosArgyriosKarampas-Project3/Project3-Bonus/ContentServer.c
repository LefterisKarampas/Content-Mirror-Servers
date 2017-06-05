#include <stdio.h>
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <unistd.h>          /* read, write, close */
#include <netdb.h>	         /* gethostbyaddr */
#include <stdlib.h>	         /* exit */
#include <string.h>	         /* strlen */
#include <netinet/in.h>	     /* internet sockets */
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "ContentServerStruct.h"
#define BUFF_SIZE 100


char *dirorfile;
pthread_mutex_t mtx;
pthread_cond_t cond_write;
pthread_cond_t cond_read;
int reader_counter = 0;
int writer_counter = 0;
HashTable HT;
int sock;

void * ContentSlave (void * );

void catchkill(int signo){
	fprintf(stderr,"ContentServer: is about time to exit\n");
	close(sock);
	HT_Clear(&HT);
	free(dirorfile);
	exit(0);
}

int main(int argc,char *argv[]){
	int port,i;
	struct sockaddr_in client;
	socklen_t clientlen;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    /* Get args */
	if(argc!=5){
		fprintf(stderr,"Usage error: ./ContentServer -p <port> -d <dirorfilename>\n");
		exit(1);
	}
	for(i=1;i<argc;i+=2){
		if(!strcmp(argv[i],"-p")){
			port = atoi(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-d")){
			dirorfile = malloc(sizeof(char)*(strlen(argv[i+1])+1));
			strcpy(dirorfile,argv[i+1]);
		}
		else{
			fprintf(stderr,"Usage error: ./ContentServer -p <port> -d <dirorfilename>\n");
			exit(1);
		}
	}
	if(chdir(dirorfile) < 0){
		perror("chdir");
		exit(1);
	}

	static struct sigaction act;
	act.sa_handler=catchkill;
	sigfillset(&(act.sa_mask));
	sigaction(SIGINT,&act,NULL);										//SIGTERM handler


	initialize(&HT);													/* Initialize HashTable_delay's struct */
    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&cond_read, 0);									/* Initialize cond_var for readers */
	pthread_cond_init(&cond_write, 0);									/* Initialize cond_var for writers */
	/* Create socket */
	sock = Create_Socket(port);											/* Create the socket for connections */
	pthread_t thread;
	Info * info;
	/* Wait for clients */
	while(1){
		info = malloc(sizeof(Info));
		clientlen = sizeof(clientptr);
		if ((info->socket = accept(sock, (struct sockaddr *)clientptr, &clientlen)) < 0){	/* New MirrorInitiator want help */
	    	perror("accept");
	  		exit(2);
    	}
    	/* Get client's IP address to search him in hashtable */ 
    	struct sockaddr_in * IP = (struct sockaddr_in *)clientptr;
    	info->ip = malloc(sizeof(char)*INET_ADDRSTRLEN);
    	strcpy(info->ip,inet_ntoa(IP->sin_addr));
    	/* Create a new thread for client's servive */
    	if(pthread_create(&thread,NULL,ContentSlave,(void *) info) < 0){
    		perror("create_thread");
    		exit(1);
    	}
    	//pthread_detach(thread);
	}
	HT_Clear(&HT);
	free(dirorfile);
}

void * ContentSlave (void * x){
	Info * info = x;
	int socket = info->socket;
	char buf[1];
	char buffer[BUFF_SIZE];
	int delay;
	int id;
	char order[6];
	char *path;
	int n,count;
	count = 0;
	int y = 1;
	int k;
	pthread_detach(pthread_self());
	/* Read what client wants to do for him */
	while(1){
		if((n = read(socket,buf,1)) > 0){
			if(buf[0] == '\0'){
				buffer[count] = '\0';
				if(k == 1)
					delay = atoi(buffer);
				else
				{
					path = malloc(sizeof(char)*(count+1));
					strcpy(path,buffer);
				}
				break;
			}
			else if((buf[0] == ' ') && (y<3)){
				buffer[count] = '\0';
				if(y == 1){
					strcpy(order,buffer);
					if(order[0] == 'F'){
						k = 0;
					}
					else if(order[0] == 'L'){
						k = 1;
					}
					else{
						fprintf(stderr,"Fail order to execute!\n");
						pthread_exit((void *)1);
					}
				}
				else{
					id = atoi(buffer);
				}
				y++;
				count = 0;
				bzero(buffer, strlen(buffer));
			}
			else{
				if(buf[0] == ' '){
					buffer[count++] = '\\';
				}
					buffer[count++] = buf[0];
			}
		}
	}
	FILE	*sock_fp = NULL;             	/* stream for socket IO */
    FILE	*pipe_fp = NULL;	           /* use popen to run ls */
    char c;
    if ((sock_fp = fdopen(socket,"r+")) == NULL){
   		perror("fdopen");
   		pthread_exit((void *)1);
   	}
	if(!strcmp(order,"LIST")){
		Node * newNode = malloc(sizeof(Node));											/* Create node to save it in HT */
		newNode->key = malloc(sizeof(char)*(strlen(info->ip)+sizeof(int)+2));
		sprintf(newNode->key,"%s:%d",info->ip,id);										/* Key has IP Address:uniqueID */
		newNode->delay = delay;
		//writer section
		pthread_mutex_lock(&mtx);
		while(reader_counter > 0 || writer_counter > 0){
			pthread_cond_wait(&cond_write,&mtx);
		}
		writer_counter++;
		pthread_mutex_unlock(&mtx);
		HT_Insert(HT,newNode);															/* Insert new node in HT */
		pthread_mutex_lock(&mtx);
		writer_counter--;
		pthread_cond_signal(&cond_write);
		pthread_cond_broadcast(&cond_read);
		pthread_mutex_unlock(&mtx);

		if ((pipe_fp = popen("find -type d -empty |cut -b 3-" , "r")) == NULL ){		/* empty folders */
	    	perror("popen");
	   		pthread_exit((void *)1);
   		}
   		while( (c = getc(pipe_fp)) != EOF )
    		putc(c, sock_fp);															/* send to client*/
    	pclose(pipe_fp);

    	if ((pipe_fp = popen("echo -f ;find -type f | cut -b 3-", "r")) == NULL ){		/* all files */
	    	perror("popen");
	   		pthread_exit((void *)1);
   		}
   		while( (c = getc(pipe_fp)) != EOF )
    		putc(c, sock_fp);															/* send to client*/
	}
	else if(!strcmp(order,"FETCH")){
		char *command;
		command = malloc(sizeof(char)*(strlen("cat ")+strlen(path)+1));
		sprintf(command,"cat %s",path);													/* Make the command for popen() */
		if ((pipe_fp = popen(command, "r")) == NULL ){
	    	perror("popen");
	   		pthread_exit((void *)1);
   		}
   		char *unid = malloc(sizeof(char)*(strlen(info->ip)+sizeof(int)+2));
		sprintf(unid,"%s:%d",info->ip,id);												/* Key for searching HT */
		//Reader section
		pthread_mutex_lock(&mtx);
		while(writer_counter > 0){
			pthread_cond_wait(&cond_read,&mtx);
		}
		reader_counter++;
		pthread_mutex_unlock(&mtx);
		int s = HT_GetDelay(HT,unid);													/* Find the delay time for a key */
		pthread_mutex_lock(&mtx);
		reader_counter--;
		if(reader_counter == 0){
			pthread_cond_signal(&cond_write);
		}
		pthread_mutex_unlock(&mtx);
		//fprintf(stderr,"Sleep %d\n",s);
   		sleep(s);																		/* Delay s secs */
   		while( (c = getc(pipe_fp)) != EOF ){
    		putc(c, sock_fp);															/* Send file to client */
   		}
   		free(unid);
   		free(path);
   		free(command);
	}
	/* transfer data from order to socket */
	pclose(pipe_fp);
    fclose(sock_fp);
    close(socket);
	free(info->ip);
	free(info);
	pthread_exit((void *)1);
}