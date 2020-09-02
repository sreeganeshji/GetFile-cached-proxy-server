/*
 *  This file is for use by students to define anything they wish.  It is used by the proxy cache implementation
 */
 #ifndef __CACHE_STUDENT_H__
 #define __CACHE_STUDENT_H__

 #include "steque.h"
 #include "shm_channel.h"
#include<sys/time.h>

#define TRUE 1
#define FALSE 0

#define DEBUG 0
#define DEBUG_time 0

typedef unsigned int boolean_t;

typedef struct cacheID {
    int id;
    boolean_t free;
} cacheID;

typedef struct handlerArg {
    msgQue* messageQueue;
    shmSet** shmArray;
    pthread_mutex_t shmQueueMutex;
    steque_t* freeQue;
    pthread_cond_t freeCacheBlocks;
    size_t segSize;
} handlerArg;

typedef struct cacheArg {
    msgQue* messageQueue;
    shmSet** shmArray;
    size_t segSize;
} cacheArg;

cacheID* getFreeCacheBlock(steque_t* freeQue);
void addFreeCacheBlock(steque_t* freeQue,cacheID* freeBlock);
 
 #endif // __CACHE_STUDENT_H__
