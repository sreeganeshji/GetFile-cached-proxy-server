//In case you want to implement the shared memory IPC as a library...

#ifndef __SHM_CHANNEL__
#define __SHM_CHANNEL__

#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/msg.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include <errno.h>

#define SERVER_MSG_INIT 3
#define CACHE_MSG_INIT 4
#define SERVER_MSG 100
#define CACHE_MSG 200
#define SERVER_SEM 0
#define CACHE_SEM 1

typedef struct shmSet
{
    key_t uniqueKey;
    int shmid;
    char* data;
    int semid;
    
} shmSet;

union semHolder
{
    int val;
};
int setSem(shmSet* thisSet,int type,int val);

void destroyShm(int count, shmSet** shm);

typedef struct msgQue {
    key_t uniqueKey;
    int msgid;
    char* data;
}msgQue;

struct msgContainer {
long mtype;     /* message type, must be > 0 */
char mtext[1];  /* message data */
     };

shmSet** createShm(int count, size_t chunkSize); //execute ftok based on n, implement the numget and map the memory into a character pointer.

//need two set of queues to maintain the currently used and the free ones.


void deleteShm(shmSet** thisSet, int count);

void removeMsq(msgQue* thisQue);


//msgque
msgQue* createMsq();
void sendMsg(msgQue* thisQue,char* msg, size_t msgLen, int type);
void receiveMsg(msgQue* thisQue, char* buffer, size_t BUFLEN, int type);

#endif

