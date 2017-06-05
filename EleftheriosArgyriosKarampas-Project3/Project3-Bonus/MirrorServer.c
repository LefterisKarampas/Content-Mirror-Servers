#include <stdio.h>
#include <sys/wait.h>	     /* sockets */
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <netdb.h>	         /* gethostbyaddr */
#include <unistd.h>	
#include <stdlib.h>	         
#include <signal.h>          /* signal */
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include "MirrorServerStruct.h"

#define BUFF_REQS 15
#define SIZE 100


pthread_mutex_t mtx;									/* Mutex for shared items */
pthread_mutex_t worker_mtx;								/* Mutex for worker's shared items */
pthread_mutex_t manager_mtx;							/* Mutex for manager's shared items */						
pthread_cond_t cond_nonempty;							/* Cond_var for empty buffer */
pthread_cond_t cond_nonfull;							/* Cond_var for full buffer */
pthread_cond_t cond_alldone;							/* Cond_var for finished the job */
int alldone = 0;
TransInfo Info;						
int FilesTransferred = 0;
int NumFilesReq = 0;
int NumFilesGet;
int NumDirs = 0;
int ManagersOut = 0;
int NumManagers = 1;
int FinishFlag = 0;
int NumWorkers;
int WorkersOut = 0;
int numDevicesDone = 0;
pool_t pool;											/* Buffer structure */


void * MirrorManager(void * ptr){
	request *x = ptr;									/* Get request structure */
	int sock,n;				
	char buff[1];	
	char *fileq = malloc(sizeof(char)*SIZE);			
	int q = 1;
	int file_flag = 0;
	int count=0;
	int dirs=0;
   	if((sock = ManagerJob(x)) < 0){						/* Try connect to ContentServer */
   		pthread_mutex_lock(&manager_mtx);				/* If fails inform all relevant structures */
		ManagersOut++;									/* and exit */
		if(ManagersOut == NumManagers){
			pthread_mutex_lock(&mtx);
			pool.count++;
			FinishFlag = 1;
			pthread_cond_broadcast(&cond_nonempty);
			pthread_mutex_unlock(&mtx);
		}
		fprintf(stderr,"Manager %d out\n",ManagersOut);
		pthread_mutex_unlock(&manager_mtx);
		free(x->Address);
		free(x->dirorfile);
		free(x);
		free(fileq);
		shutdown(sock,SHUT_RDWR);
		pthread_exit((void *)2);
   	}
   	/* Connect to ContentServer successfully */
   	pthread_mutex_lock(&manager_mtx);
   	numDevicesDone++;
   	pthread_mutex_unlock(&manager_mtx);
   	/* Read empty dirs and files from LIST */
    while( (n = read(sock, buff, 1)) > 0 ){
    	if(count == SIZE *q){
        	q++;
        	fileq = realloc(fileq,sizeof(char)*SIZE*q);
        }
        if(buff[0] != '\n'){
        	fileq[count] = buff[0];						/* Copy each byte until to newline */
        	count++;
        }
        else{
        	fileq[count] = '\0';
        	if(!strcmp(fileq,"-f")){					/* Flag for files reading */
        		file_flag = 1;
        	}
        	count++;
        	char *storefile;
        	if(filter_file(fileq,x->dirorfile,file_flag,&storefile,&dirs)){		/* Check if file/dir is the requested */
	        	if(file_flag){													/* if file_flag is on */
        			/* Create request for buffer */
				    file_req *file = malloc(sizeof(file_req));
				    file->filename = malloc(sizeof(char)*count);
				    strcpy(file->filename,fileq);
				    file->Address = malloc(sizeof(char)*(strlen(x->Address)+1));
				    strcpy(file->Address,x->Address);
				    file->storefile = storefile;
				    file->port = x->port;
				    file->id = x->id;
				    /* Try store a request in buffer */
				    pthread_mutex_lock(&mtx);
				    while(pool.count >= BUFF_REQS){
				    	pthread_cond_wait(&cond_nonfull,&mtx);
				    }
				    NumFilesReq++;
				    pool.end = (pool.end +1 ) % BUFF_REQS;
				    pool.Buffer[pool.end] = file;
				    pool.count++;
				    pthread_cond_signal(&cond_nonempty);
					pthread_mutex_unlock(&mtx);
				}
			}
			bzero(fileq, strlen(fileq)); 
			count = 0;
		}
	}
	/* MirrorManager ready to exit */
	pthread_mutex_lock(&manager_mtx);
	ManagersOut++;										/* Increase the num of managers which are finished */
	NumDirs+=dirs;										/* Increase the num of dirs which have been created */
	if(ManagersOut == NumManagers){						/* If last manager is finished, have to inform FinishFlag */
		pthread_mutex_lock(&mtx);						
		pool.count++;									/* Increase buffer's count in order to avoid the deadlock on workers */
		FinishFlag = 1;
		pthread_cond_broadcast(&cond_nonempty);
		pthread_mutex_unlock(&mtx);
	}
	pthread_mutex_unlock(&manager_mtx);
	fprintf(stderr,"Manager %d out\n",ManagersOut);
	/* Deallocate structure and exit */
	free(x->Address);
	free(x->dirorfile);
	free(x);
	free(fileq);
	shutdown(sock,SHUT_RDWR);
	close(sock);
	pthread_exit((void *)1);
};

void * worker(void * ptr){
	while(1){
		/* Try get a request from buffer */
		pthread_mutex_lock(&mtx);
		while(pool.count <= 0){													/* If buffer is empty */
			pthread_cond_wait(&cond_nonempty,&mtx);								/* Suspend */
		}
		if(FinishFlag && (NumFilesReq == NumFilesGet)&&(pool.count == 1)){		/* Check if job has finished */
			pthread_cond_broadcast(&cond_nonempty);
			pthread_mutex_unlock(&mtx);
			break;																/* Get out from inf loop */
		}
		/* Get a request from buffer */
		file_req * x = pool.Buffer[pool.start];
		pool.start = (pool.start +1) % BUFF_REQS;
		pool.count--;
		NumFilesGet++;
		pthread_cond_signal(&cond_nonfull);										/* Wake up managers if they are suspended */
		pthread_mutex_unlock(&mtx);
		long int bytes;
		bytes = WorkerJob(x);													/* Fetch file */
		pthread_mutex_lock(&worker_mtx);										/* Update the workers' info variable */
		if(bytes >= 0){
			List_Insert(&Info,bytes);
			FilesTransferred +=1;
		}
		pthread_mutex_unlock(&worker_mtx);
	}

	pthread_mutex_lock(&worker_mtx);
	WorkersOut++;															
	fprintf(stderr,"Worker %d out\n",WorkersOut);
	/* If all workers are out, wake up main */
	if(NumWorkers == WorkersOut){
		fprintf(stderr,"Last worker out\n");
		alldone = 1;
		pthread_cond_signal(&cond_alldone);
	}
	pthread_mutex_unlock(&worker_mtx);
	pthread_exit((void *)1);
};


int main(int argc,char *argv[]){
	struct sockaddr_in client;
    socklen_t clientlen;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    //struct hostent *rem;
	int port, sock,newsock;
	char *dirname;
	pthread_t *workers;
	pthread_t *managers;
	int i,err;

	Get_Args(argc,argv,&port,&dirname,&NumWorkers);							/* Get arguments */
	struct stat st = {0};
	if(stat(dirname,&st) == -1){
		mkdir(dirname,0766);
	}
	if(chdir(dirname) < 0){													/* Change Directory */
		perror("chdir");
		exit(1);
	}
	initialize(&pool);														/* Initialize buffer's struct */
    pthread_mutex_init(&mtx, 0);											/* Initialize mutex */
    pthread_mutex_init(&worker_mtx, 0);										/* Initialize worker_mutex */
    pthread_mutex_init(&manager_mtx, 0);									/* Initialize manager_mutex */
	pthread_cond_init(&cond_nonempty, 0);									/* Initialize cond_var non_empty*/
	pthread_cond_init(&cond_nonfull, 0);									/* Initialize cond_var non_full*/
	pthread_cond_init(&cond_alldone, 0);									/* Initialize cond_var all_done*/
	if ((workers = malloc(NumWorkers * sizeof(pthread_t))) == NULL) {		/* Create worker table structure */
    	perror("malloc");
    	exit(1);
    }
	for(i=0;i<NumWorkers;i++){												/* Create worker threads */
		if ((err = pthread_create(workers+i,NULL,worker,NULL))){
			perror2("pthread_worker_create",err); 
			exit(1);
		}
	}
	List_Initialize(&Info);
	sock = Create_Socket(port);											/* Create socket */
    /* accept connection */
    clientlen = sizeof(clientptr);
    if ((newsock = accept(sock, (struct sockaddr *)clientptr, &clientlen)) < 0){	/* New MirrorInitiator request */
    	perror("accept");
  		exit(2);
    }
    managers = Request(newsock,&NumManagers);					/* Create Managers for each request */

    if ((err = pthread_mutex_lock(&mtx))) { 					/* Lock mutex */
    	perror2("pthread_mutex_lock", err);
    	exit(1);
    }
    while(!alldone)
    	pthread_cond_wait(&cond_alldone, &mtx);						/*Wait until the whole job finished*/
    fprintf(stderr,"Main out\n");
    pthread_mutex_unlock(&mtx);										/* Unlock mutex */
    
	for(i=0;i<NumManagers;i++){
		if ((err = pthread_join(managers[i],NULL))){
			perror2("pthread_MirrorManager_join",err); 
			exit(1);
		}
	}
	for(i=0;i<NumWorkers;i++){
		if ((err = pthread_join(*(workers+i),NULL))){
			perror2("pthread_worker_join",err); 
			exit(1);
		}
	}
	/* Send results to MirrorInitiator */
	char buf[100];
    sprintf(buf,"BytesTransferred: %ld\n",Info.BytesTransferred);
    write_all(newsock,buf,strlen(buf)+1);
    bzero(buf,strlen(buf));
    sprintf(buf,"FilesTransferred: %d\n",FilesTransferred);
   	write_all(newsock,buf,strlen(buf)+1);
   	bzero(buf,strlen(buf));
    sprintf(buf,"DirsTransferred: %d\n",NumDirs);
   	write_all(newsock,buf,strlen(buf)+1);
   	bzero(buf,strlen(buf));
   	double mean;
   	if(FilesTransferred > 0)
   		mean = ((double)Info.BytesTransferred)/FilesTransferred;
   	else 
   		mean = 0;
  	sprintf(buf,"Mean BytesTransferred per file: %.0f\n",mean);
   	write_all(newsock,buf,strlen(buf)+1);
   	bzero(buf,strlen(buf));
   	double dispresion = 0.0;
   	Node *temp;
   	temp = Info.head;
   	while(temp){
   		dispresion += ((double)(temp->bytes - mean))*((double)(temp->bytes - mean));
   		temp = temp->next;
   	}
  	sprintf(buf,"Dispresion: %.0lf\n",((double)dispresion)/FilesTransferred);
   	write_all(newsock,buf,strlen(buf)+1);
   	write_all(newsock,"!",1);
   	
   	/*Deallocate and delete all structure */
   	List_Clear(&Info);
	free(workers);
	free(dirname);
	free(managers);
	if ((err = pthread_cond_destroy(&cond_nonempty))) { 	/* Destroy cond_var */
    	perror2("pthread_mutex_destroy", err);
    	exit(1); 
    }
	if ((err = pthread_cond_destroy(&cond_nonfull))) { 	/* Destroy cond_var */
    	perror2("pthread_mutex_destroy", err);
    	exit(1); 
    }
    if ((err = pthread_cond_destroy(&cond_alldone))) { 	/* Destroy cond_var */
    	perror2("pthread_mutex_destroy", err);
    	exit(1); 
    }
	if ((err = pthread_mutex_destroy(&mtx))) { 			/* Destroy mutex */
    	perror2("pthread_mutex_destroy", err);
    	exit(1); 
    }
    if ((err = pthread_mutex_destroy(&worker_mtx))) { 	/* Destroy worker_mutex */
    	perror2("pthread_mutex_destroy", err);
    	exit(1); 
    }
    if ((err = pthread_mutex_destroy(&manager_mtx))) { 	/* Destroy manager_mutex */
    	perror2("pthread_mutex_destroy", err);
    	exit(1); 
    }
    shutdown(newsock,SHUT_RDWR);
    close(newsock);
    shutdown(sock,SHUT_RDWR);
	close(sock);
	return 0;

}