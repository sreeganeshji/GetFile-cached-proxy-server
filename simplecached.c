#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <sys/signal.h>
#include <printf.h>
#include <curl/curl.h>

#include "gfserver.h"
#include "cache-student.h"
#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 5041

static shmSet** cacheSet;
static int nsegments;
static msgQue* cacheMsq;

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		// you should do IPC cleanup here
        printf("caught signal %d\n",signo);
        deleteShm(cacheSet,nsegments);
        removeMsq(cacheMsq);
        simplecache_destroy();
		exit(signo);
	}
}

unsigned long int cache_delay;

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 7, Range is 1-31415)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-5000000 (microseconds)\n "	\
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

//Cache thread handler
/*
 resources needed for the thread handler.
 1. Mutex to read the resource.
 * we are not writing to the resource apart from the time of initialization. hence we don't need to protect it from multiple reads.
 * mutex to access the request queue? We need a message passing interface to pass the requests. Since the threads are all that are running, we need to have the main loop act as the boss and add the requests into the queue, but how will the
 */

static int cacheCount = 1;

void* cacheThread(void* arg)
{
    int thisCache = cacheCount++;
    cacheArg* thisArg = (cacheArg*) arg;
    char Message[100];
    while(1)
    {
        //timestamp
        struct timeval thisTime;
          gettimeofday(&thisTime, NULL);
    if (DEBUG_time)  printf("cache%d: initialization end at s: %ld, us:%ld\n",thisCache,thisTime.tv_sec,thisTime.tv_usec);
    
    //struct msgContainer* thisMessage = (struct msgContainer*) malloc(sizeof(struct msgContainer)+100+1);
        
        memset(Message, 0, 100);
        struct msgContainer* thisMessage = (struct msgContainer*) Message;
    strcpy(thisMessage->mtext, "") ;
    receiveMsg(thisArg->messageQueue, (char*)thisMessage, 100,SERVER_MSG);
    while(strcmp(Message, "") == 0)
        receiveMsg(thisArg->messageQueue, (char*)thisMessage, 100,SERVER_MSG);

    char* bufferM = thisMessage->mtext;
    char* word;
    char* path;
    char* strptr;

    word = strtok_r(bufferM, " ",&strptr);
    word = strtok_r(NULL, " ",&strptr);
    path = word;
    word = strtok_r(NULL, " ",&strptr);
    word = strtok_r(NULL, " ",&strptr);
        char* ptr;
        long freeCache = strtol(word, &ptr, 10);
        if(word == ptr)
        {
            printf("cache%d: strtol fault, word %s\n",thisCache,word);
            break;
        }
 
//    query the file.
    int fildes = simplecache_get(path);
 //       free(thisMessage);
    if (fildes <0)
    {
// couldn't find the file.
        sendMsg(thisArg->messageQueue, "NOT_FOUND", 20, CACHE_MSG+freeCache);
    }
    else{
        //timestamp
        struct timeval thisTime;
          gettimeofday(&thisTime, NULL);
         if (DEBUG_time) printf("cache%d: initialization end %s at s: %ld, us:%ld\n",thisCache,path,thisTime.tv_sec,thisTime.tv_usec);
        
        /* Calculating the file size */
        size_t file_len, bytes_transferred;
        ssize_t read_len;
        struct stat statbuf;
        int fstatStatus = fstat(fildes, &statbuf);
        if (0 > fstatStatus) {
            printf("couldn't get the stats due to error %s\n",strerror(errno));
            
            break;
        }

        file_len = (size_t) statbuf.st_size;
        char msg[100];
        snprintf(msg, 100, "FOUND %zu",file_len);
        if(DEBUG) printf("cache%d:%s %s\n",thisCache,path,msg);
        sendMsg(thisArg->messageQueue, msg, 100, CACHE_MSG+freeCache);
        

         //timestamp
          gettimeofday(&thisTime, NULL);
         if (DEBUG_time) printf("cache%d: transfer begin %s at s: %ld, us:%ld\n",thisCache,path,thisTime.tv_sec,thisTime.tv_usec);
        /* Sending the file contents chunk by chunk. */
        
        bytes_transferred = 0;
//        if(DEBUG) printf("cache%d: reading\n",thisCache);
        while(bytes_transferred < file_len){
            size_t segFilled = 0;
            while(bytes_transferred < file_len && segFilled < thisArg->segSize)
            {
                read_len = pread(fildes, (thisArg->shmArray[freeCache]->data+segFilled), thisArg->segSize-segFilled, bytes_transferred);
                
           // read_len = read(fildes, (thisArg->shmArray[freeCache].data+segFilled), thisArg->segSize-segFilled);
            if (read_len <= 0){
                printf( "cache%d: handle_with_file read error, %zd, %zu, %zu",thisCache, read_len, bytes_transferred, file_len );
                break;
            }
                segFilled += read_len;
                bytes_transferred += read_len;
//               if(DEBUG) printf("client: %ld\n",bytes_transferred);
            }
            setSem(thisArg->shmArray[freeCache], SERVER_SEM, 1);
            setSem(thisArg->shmArray[freeCache], CACHE_SEM, -1);
            
        }
        if (DEBUG) printf("cache%d: bytes transferred %zu\n",thisCache,bytes_transferred);
        //timestamp
          gettimeofday(&thisTime, NULL);
         if (DEBUG_time) printf("cache%d: transfer end %s at s: %ld, us:%ld\n",thisCache,path,thisTime.tv_sec,thisTime.tv_usec);
    }
    
    }
    return 0;
    
}

void getCacheInfo(msgQue* thisQue, int* nosegment, size_t* segsize)
{
    struct msgContainer* thisMessage = (struct msgContainer*) malloc(sizeof(struct msgContainer)+100+1);
    strcpy(thisMessage->mtext, "") ;
    while(strcmp(thisMessage->mtext, "") == 0)
    {
        receiveMsg(thisQue, (char*)thisMessage, 100,SERVER_MSG_INIT);
    }
    char* buffer = thisMessage->mtext;
    char* word;
    char* strptr;
    word = strtok_r(buffer, " ",&strptr);
    for( int i = 0 ; i <= 3; i++)
    {
        word = strtok_r(NULL, " ",&strptr);
        if (i == 1)
        {
            *nosegment = atoi(word);
        }
        if(i == 3)
        {
            *segsize = atoi(word);
        }
    }
    free(thisMessage);
    sendMsg(thisQue, "received", 20, CACHE_MSG_INIT);
}


int main(int argc, char **argv) {
	int nthreads = 7;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "id:c:hlxt:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;
			case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'l': // experimental
			case 'x': // experimental
				break;
		}
	}

	if (cache_delay > 5000000) {
		fprintf(stderr, "Cache delay must be less than 5000000 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>31415) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	// Initialize cache
	simplecache_init(cachedir);

    // cache code gos here
    cacheMsq = createMsq();
    size_t segsize = 0;
    getCacheInfo(cacheMsq,&nsegments,&segsize);
    cacheSet = createShm(nsegments, segsize);
    cacheArg cacheThreadArg;
    cacheThreadArg.messageQueue = cacheMsq;
    cacheThreadArg.shmArray = cacheSet;
    cacheThreadArg.segSize = segsize;

    pthread_t cacheThreads[nthreads];
    
    for(int i = 0 ; i < nthreads; i++)
    {
       int threadCond = pthread_create(&cacheThreads[i], NULL, cacheThread, &cacheThreadArg);
        
        if (threadCond < 0)
        {
            printf("couldn't create threads\n");
            break;
        }
    }
    
    //keep serving
    while(1)
    {
        
    };

	// Won't execute
	return 0;
}
