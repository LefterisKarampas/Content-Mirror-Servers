CC = gcc
CFLAGS = -g -Wall
TARGET = MirrorServer ContentServer MirrorInitiator

.PHONY: clean all

all: MirrorServer ContentServer MirrorInitiator

MirrorServerFunctions.o: MirrorServerFunctions.c 
	$(CC) $(CFLAGS) -c MirrorServerFunctions.c -o MirrorServerFunctions.o

MirrorServer.o: MirrorServer.c MirrorServerFunctions.c
	$(CC) $(CFLAGS) -c MirrorServer.c -o MirrorServer.o

MirrorServer: MirrorServer.o MirrorServerFunctions.o
	$(CC) $(CFLAGS)  MirrorServer.o MirrorServerFunctions.o -o MirrorServer -lpthread

ContentServer.o: ContentServer.c
	$(CC) $(CFLAGS) -c ContentServer.c -o ContentServer.o

ContentServerFunctions.o: ContentServerFunctions.c
	$(CC) $(CFLAGS) -c ContentServerFunctions.c -o ContentServerFunctions.o

ContentServer: ContentServer.o ContentServerFunctions.o
	$(CC) $(CFLAGS)  ContentServer.o ContentServerFunctions.o -o ContentServer -lpthread

MirrorInitiator.o: MirrorInitiator.c 
	$(CC) $(CFLAGS) -c MirrorInitiator.c -o MirrorInitiator.o


MirrorInitiator: MirrorInitiator.o 
	$(CC) $(CFLAGS) MirrorInitiator.o -o MirrorInitiator

	rm -rf *.o
clean:
	rm -rf *.o $(TARGET)


