#include <stdio.h>
#include <sys/wait.h>	     /* sockets */
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <unistd.h>	
#include <stdlib.h>	
#include <pthread.h>
#include <netdb.h>	         /* gethostbyaddr */         
#include <string.h>
#include <sys/stat.h>		 /* stat for mkdir */
#include "MirrorServerStruct.h"

#define LISTEN 3
#define SIZE 100

void Get_Args(int argc,char **argv,int *port, char **dir,int *threads){
	if(argc!=7){
		fprintf(stderr,"Usage error: ./MirrorServer -p <port> -m <dirname> -w <threadnum>\n");
		exit(1);
	}
	int i;
	for(i=1;i<argc;i+=2){
		if(!strcmp(argv[i],"-p")){
			*port = atoi(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-m")){
			*dir = malloc(sizeof(char)*(strlen(argv[i+1])+1));
			strcpy(*dir,argv[i+1]);
		}
		else if(!strcmp(argv[i],"-w")){
			*threads = atoi(argv[i+1]);
		}
		else{
			fprintf(stderr,"Usage error: ./MirrorServer -p <port> -m <dirname> -w <threadnum>\n");
			exit(1);
		}
	}
}

/* Initialize Buffer Structure */
void initialize(pool_t *pool){
	pool->start = 0;
	pool->end = -1;
	pool->count = 0;
}

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

/* Delis' write_all function */
int write_all(int fd, void *buff, size_t size) {
    int sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1) 
            return -1; /* error */
    }
    return sent;
}

/* Read requests from MirrorInitiator and create MirrorManagers to work on them */
pthread_t * Request(int newsock,int *managers){
	char buf[1];
    char order[SIZE];
    int counter = 0;
    int index = 0;
    int count_managers = 0;
    int n = 1;
    fprintf(stderr,"Someone connect!\n");
    request **y;
    pthread_t *x;
    y = malloc(sizeof(request *)*10);                       /* Requests structure */
    x = malloc(sizeof(pthread_t)*10);                       /* Array of MirrorManagers' thread_id */
    /* Create request for MirrorManagers */
    while(1){
    	if(read(newsock, buf, 1) > 0){
    		if(buf[0] == '!')
    			break;
    		else if(buf[0] == '\n'){
    			y[count_managers]->delay = atoi(order);
    			y[count_managers]->id = count_managers+1;
    			count_managers++;
    			if(count_managers == 10 * n){
    				n++;
    				if((x = realloc(x,sizeof(pthread_t)*10*n)) == NULL){
    					perror("realloc");
    					exit(3);
    				}
                    if((y = realloc(y,sizeof(request *)*10*n)) == NULL){
                        perror("realloc");
                        exit(3);
                    }
    			}
    			counter = 0;
    			index = 0;
    		}
    		else if(buf[0] == ':'){
    			counter+=1;
    			order[index] = '\0';
    			switch(counter){
    				case 1:
                        y[count_managers] = malloc(sizeof(request));
    					y[count_managers]->Address = malloc(sizeof(char)*(strlen(order)+1)); 
    					strcpy(y[count_managers]->Address,order);
    					break;
    				case 2: 
    					y[count_managers]->port = atoi(order);
    					break;
    				case 3:
    					y[count_managers]->dirorfile = malloc(sizeof(char)*(strlen(order)+1)); 
    					strcpy(y[count_managers]->dirorfile,order);
    			}
    			index = 0;
    		}
    		else{
    			order[index] = buf[0];
    			index++;
    		}
    	}
    }
    int err;
    fprintf(stderr, "Create %d MirrorManager\n",count_managers);
    *managers = count_managers;
    /* Create MirrorManager threads and give as param, a request to work on it */
    for(n =0;n<count_managers;n++){
        if((err = pthread_create(&(x[n]),NULL,MirrorManager,(void *)y[n]))){
            perror2("pthread_worker_create",err); 
            exit(1);
        }
    }
    free(y);
    return x;   /* Return array of threads' id */
}

/* MirrorManager connect and send LIST request to ContentServer */
int ManagerJob(request * x){
	int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("connect");
    	return -1;
    }
    if ((rem = gethostbyname(x->Address)) == NULL) {	
		herror("gethostbyname");
		return -1;
    }
    server.sin_family = AF_INET;       							  /* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(x->port);        						/* Server port */
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0){
    	fprintf(stderr,"Fail connect to %s in %d port\n",x->Address,x->port);
    	return -1;
    }
    char buff[SIZE];
    sprintf(buff,"LIST %d %d",x->id,x->delay);
    write_all(sock,buff,strlen(buff)+1);
    return sock;
}

/* Worker connect to ContentServer and fetch a file in order to store it in local space */
long int WorkerJob(file_req *x){
	int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("connect");
    	return -1;
    }
    if ((rem = gethostbyname(x->Address)) == NULL) {	
		herror("gethostbyname");
		return -1;
    }
    server.sin_family = AF_INET;       							/* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(x->port);        						/* Server port */
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0){
    	fprintf(stderr,"Fail connect to %s in %d port\n",x->Address,x->port);
    	free(x->Address);
        free(x->storefile);
        free(x->filename);
        free(x); 
    	return -1;
    }
    char buff[SIZE];
    sprintf(buff,"FETCH %d %s",x->id,x->filename);
    write_all(sock,buff,strlen(buff)+1);
    int bytes = 0;
    FILE *fp = fopen(x->storefile,"w");                             /* Creates and/or open a file for writting */
    if(fp == NULL){
        perror("fp");
        free(x->Address);
        free(x->storefile);
        free(x->filename);
        free(x); 
        return -1;
    }
    /* Delis' way for getting a message */
    FILE *sock_fp;
    char c;
    if ((sock_fp = fdopen(sock,"r")) == NULL){
        perror("fdopen");
        free(x->Address);
        free(x->storefile);
        free(x->filename);
        free(x); 
        return -1;
   	}
   	while( (c = getc(sock_fp)) != EOF ){
   		putc(c,fp);
   		bytes+=1;
   	}
    /* Delete/remove all structure */
    free(x->Address);
    free(x->storefile);
    free(x->filename);
    free(x); 
    fclose(sock_fp);
    fclose(fp);
    shutdown(sock,SHUT_RDWR);
    close(sock);
    return bytes;
}

/* MirrorManager check if dirofile which Initiator wants, exists in path which get from ContentServer */
int filter_file(char *src,char *dest,int file_flag,char ** storefile,int *dirs){
	char ** directory_map;
	int k1 = strlen(src);
	int k2 = strlen(dest);
	if(k1<k2){
		return 0;
    }
    directory_map = malloc(sizeof(char *)*4);       /* Hold path in case you create folders */ 
	int i;
	int flag = 0;
	char buff[100];
    char temp[100];
    int temp_count = 0;
	int count = 0;
	int m = 0;
	int k = 0;
	int mul = 1;
    /* Try matching the dirorfile to ContentServer file/dir path */
	for(i=0;i<k1;i++){
		buff[m++] = src[i]; 
		if(flag == 0){                                           /*Not match something yet */
			if(dest[k] == src[i]){
				k++;
			}
			else{
				k = 0;
			}
			if((k == k2) && (i+1<k1) && (src[i+1] == '/')){      /* Matching as dir */
				flag = 1;
			}
			else if((k == k2) && (i+1 == k1)){                   /* Matching as file/dir */
				flag = 1;
			}
		}
		else{                                                     /* Flag is 1, we have matched and now we create the path */
            if(count == 0){
                buff[m] = '\0';
                strcpy(temp,buff);
                temp_count = m;
            }
            else{
                temp[temp_count++] = src[i];
            }
			if(buff[m-1] == '/'){
				buff[m-1] = '\0';
				directory_map[count] = malloc(sizeof(char)*(strlen(buff)+1));
				strcpy(directory_map[count++],buff);
				if(count == 4*mul){
					mul++;
					directory_map = realloc(directory_map,sizeof(char *)*4*mul);
				}
				m = 0;
				bzero(buff,sizeof(buff));
			}	
		}
		if(src[i] == '/'){
			m = 0;
		}
	}

	if(flag == 0){
        free(directory_map);
		return  0;
	}
    if(!file_flag){
        buff[m] = '\0';
        directory_map[count] = malloc(sizeof(char)*(strlen(buff)+1));
        strcpy(directory_map[count++],buff);
    }
    /* Create dirs */
    struct stat st = {0};
    k = 0;
    mul = 1;
    char *folder = malloc(sizeof(char)*100);
    for(i=0;i<count;i++){
    	k += strlen(directory_map[i])+1;
    	if(k >=mul*100){
    		mul++;
    		folder = realloc(folder,sizeof(char)*mul*100);
    	}
    	if(i==0){
    		sprintf(folder,"%s",directory_map[i]);
    	}
    	else{
            char *temp = malloc(sizeof(char)*(strlen(folder)+1));
            strcpy(temp,folder);
    		sprintf(folder,"%s/%s",temp,directory_map[i]);
            free(temp);
    	}
    	if(stat(folder,&st) == -1){
    		mkdir(folder,0766);
            (*dirs)++;

    	}
    	free(directory_map[i]);
    }
    /* Hold in storefile the file's path which store in local space */
    if(file_flag){
        if(temp_count == 0){
            buff[m] = '\0';
            *storefile = malloc(sizeof(char)*(m+1));
            strcpy(*storefile,buff);
        }
        else{
            temp[temp_count] = '\0';
            *storefile = malloc(sizeof(char)*(temp_count+1));
            strcpy((*storefile),temp);
        }
    }
    free(folder);
	free(directory_map);
	return flag;
}

void List_Initialize(TransInfo *x){
    x->BytesTransferred = 0;
    x->head = NULL;
}

void List_Insert(TransInfo *x,long int bytes){
    x->BytesTransferred += bytes;
    Node * newNode;
    newNode = malloc(sizeof(Node));
    newNode->bytes = bytes;
    if(x->head){
        newNode->next = x->head;
        x->head = newNode;   
    }
    else{
        newNode->next = NULL;
        x->head = newNode;
    }
}

void List_Clear(TransInfo *x){
    Node *temp;
    while(x->head){
        temp = x->head;
        x->head = temp->next;
        free(temp);
    }
}