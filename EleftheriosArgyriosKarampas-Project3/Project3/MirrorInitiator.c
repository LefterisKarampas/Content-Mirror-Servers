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


int main(int argc,char *argv[]){
	int port,i,sock;
	char * ServerAddress;
	char * Requests;
	struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
	if(argc<7){
		fprintf(stderr,"Usage error: /MirrorInitiator -n <MirrorServerAddress> -p <MirrorServerPort> "
						"-s <ContentServerAddress1:ContentServerPort1:dirorfile1:delay1, ...\n");
		exit(1);
	}
	for(i=1;i<argc;i+=2){
		if(!strcmp(argv[i],"-p")){
			port = atoi(argv[i+1]);
		}
		else if(!strcmp(argv[i],"-n")){
			ServerAddress = malloc(sizeof(char)*(strlen(argv[i+1])+1));
			strcpy(ServerAddress,argv[i+1]);
		}
		else if(!strcmp(argv[i],"-s")){
			Requests = malloc(sizeof(char)*(strlen(argv[i+1])+1));
			strcpy(Requests,argv[i+1]);
		}
		else{
			fprintf(stderr,"Usage error: /MirrorInitiator -n <MirrorServerAddress> -p <MirrorServerPort> "
						"-s <ContentServerAddress1:ContentServerPort1:dirorfile1:delay1, ...\n");
			exit(1);
		}
	}
    /* Find MirrorServer IP address */
    if ((rem = gethostbyname(ServerAddress)) == NULL) {	
		herror("gethostbyname");
		exit(1);
    }
    /* Create socket for communication */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("connect");
        exit(1);
    }
    server.sin_family = AF_INET;       							/* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(port);        						/* Server port */
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0){
    	perror("connect");
    	exit(1);
    }
    char *str;
    int length;
    str = strtok(Requests,",");
    /* Send requests */
    while(str != NULL){
    	length = strlen(str);
        int i;
    	for(i=0;i <= length;i++){
    		if (write(sock, str+i, 1) < 0){
        	   perror("write");
        	   exit(1);
    		}
    	}
    	if (write(sock, "\n",1) < 0){
        	perror("write");
        	exit(1);
    	}
    	str = strtok(NULL,",");
    }
    if (write(sock, "!",1) < 0){
        perror("write");
        exit(1);
    }
    int n;
    char buf[1];
    /* Wait for results */
    while(1){
        if((n = read(sock,buf,1)) > 0){
            if(buf[0] == '!')
                break;
            putchar(buf[0]);
        }
    }
    shutdown(sock,SHUT_RDWR);
    close(sock);
    free(ServerAddress);
    free(Requests);
    return 0;
}