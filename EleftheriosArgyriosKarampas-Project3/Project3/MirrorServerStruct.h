#ifndef _MIRRORSERVER_STRUCT_H_
#define _MIRRORSERVER_STRUCT_H_
#define BUFF_REQS 15
#define perror2(s, e) fprintf(stderr, "%s: %s\n", s, strerror(e))

typedef struct req{
	char * Address;
	int port;
	char * dirorfile;
	int delay;
	int id;
}request;

typedef struct fil{
	char *filename;
	char *storefile;
	char *Address;
	int port;
	int id;
}file_req;

typedef struct pool
{
	file_req * Buffer[BUFF_REQS];
	int count;
	int start;
	int end;
}pool_t;

typedef struct node{
	long int bytes;
	struct node * next;
}Node;

typedef Node * List;

typedef struct transInfo{
	long int BytesTransferred;
	List head;
}TransInfo;

void * MirrorManager(void *);
int Create_Socket(int);
void Get_Args(int ,char **,int *, char **,int *);
int write_all(int, void *, size_t);
int ManagerJob(request *);
void initialize(pool_t *);
pthread_t * Request(int,int *);
long int WorkerJob(file_req *);
int filter_file(char *,char *,int,char **,int *);
void List_Initialize(TransInfo *);
void List_Insert(TransInfo *,long int);
void List_Clear(TransInfo *);
#endif //_MIRRORSERVER_STRUCT_H_