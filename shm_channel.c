//In case you want to implement the shared memory IPC as a library...

#include "shm_channel.h"



shmSet** createShm(int count, size_t chunkSize)
{
    shmSet** shmArray = malloc(sizeof(shmSet*)*count);
    
    for(int i = 0 ; i < count; i ++)
    {   shmSet* thisSet = (shmSet*) malloc(sizeof(shmSet));
        shmArray[i] = thisSet;
//        initializing shared memory
        thisSet->uniqueKey = ftok(".", 60+(2*i));
        thisSet->shmid = shmget(thisSet->uniqueKey, chunkSize, 0666|IPC_CREAT);
        thisSet->data = shmat(thisSet->shmid, 0, 0);
    
//        initializing semaphore
        thisSet->semid = semget(thisSet->uniqueKey, 2, 0666|IPC_CREAT);
        union semHolder semun;
        semun.val = 0;
        int semctlRes1 = semctl(thisSet->semid, 0, SETVAL,semun);
        int semctlRes2 = semctl(thisSet->semid, 1, SETVAL,semun);
        if(semctlRes1 < 0 || semctlRes2 < 0)
        {
            printf("couldn't initialize semaphore errno:%d and error: %s\n",errno,strerror(errno));
        }
        
        thisSet++;
    }
    return shmArray;
}


int setSem(shmSet* thisSet,int type,int val)
{
    struct sembuf semOpSt;
    semOpSt.sem_num = type;
    semOpSt.sem_op = val;
    semOpSt.sem_flg = 0;
    return semop(thisSet->semid,&semOpSt,1);
}

void deleteShm(shmSet** thisSet, int count)
{
    for(int i = 0 ; i < count; i++)
    {
        shmdt(thisSet[i]->data);
        semctl(thisSet[i]->semid, 0, IPC_RMID);
        shmctl(thisSet[i]->shmid, IPC_RMID, NULL);
        free(thisSet[i]);
    }
}

msgQue* createMsq()
{   msgQue* thisQue = (msgQue*)malloc(sizeof(msgQue));
    thisQue->uniqueKey = ftok(".", 10);
    thisQue->msgid = msgget(thisQue->uniqueKey, 0666|IPC_CREAT);
    return thisQue;
}

void removeMsq(msgQue* thisQue)
{
    msgctl(thisQue->msgid, IPC_RMID, NULL);
}
   
void sendMsg(msgQue* thisQue,char* msg, size_t msgLen,int type)
{
//    struct msgbuf* thisMsg = (struct msgbuf*)malloc(sizeof(struct msgbuf)+msgLen+1);
    struct msgContainer* thisMsg = (struct msgContainer *) malloc(sizeof(struct msgContainer) + msgLen+1);
    thisMsg->mtype = type;
//    thisMsg->mtext = (char*) malloc(msgLen+1);
    strcpy(thisMsg->mtext, msg);
    msgsnd(thisQue->msgid, thisMsg, strlen(thisMsg->mtext)+1, 0);
    free(thisMsg);
}

void receiveMsg(msgQue* thisQue, char* buffer, size_t BUFLEN,int type)
{
    msgrcv(thisQue->msgid, buffer, BUFLEN, type, 0);
}


