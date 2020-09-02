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

#include "gfserver.h"
#include "cache-student.h"

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */                         
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 8)\n"                      \
"  -p [listen_port]    Listen port (Default: 19121)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 11, Range: 1-1219)\n"              \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"     \
"  -z [segment_size]   The segment size (in bytes, Default: 8192).\n"                  \
"  -h                  Show this help message\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"segment-count", required_argument,      NULL,           'n'},
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},
  {"hidden",        no_argument,            NULL,           'i'}, /* server side */
  {NULL,            0,                      NULL,            0}
};

static shmSet** serverCache;
static unsigned int nsegments;
static msgQue* serverMsq;
static steque_t* serverSteque;

void freeQue(steque_t* thisQue);

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;

static void _sig_handler(int signo){
    printf("caught signal %d\n",signo);
  if (signo == SIGTERM || signo == SIGINT){
      deleteShm(serverCache, nsegments);
      removeMsq(serverMsq);
    gfserver_stop(&gfs);
      freeQue(serverSteque);
    exit(signo);
  }
}

void wakeUpCacheDaemon(msgQue* thisQue, int Nseg, size_t segSize)
{
    char msg[30];
    snprintf(msg, 30, "cache: Nseg %d segSize %zu",Nseg,segSize);
    sendMsg(thisQue, msg, 30,SERVER_MSG_INIT);
    char ack[10] = "";
    while(strcmp(ack, "") == 0)
    {
    receiveMsg(thisQue, ack, 10,CACHE_MSG_INIT);
    }
}

void freeQue(steque_t* thisQue)
{
    cacheID* thisID;
    while(!steque_isempty(thisQue))
    {
        thisID = steque_pop(thisQue);
        free(thisID);
    }
}

void freeQueInit(steque_t* freeQue, int nsegments){
    steque_init(freeQue);
    for(int i = 0; i < nsegments; i++)
    {
        cacheID* newID = (cacheID*) malloc(sizeof(cacheID));
        newID->free = TRUE;
        newID->id = i;
        steque_enqueue(freeQue, (void*)newID);
    }
}

//returns the first free cache block
cacheID* getFreeCacheBlock(steque_t* freeQue)
{
    cacheID* freeCacheBlock = steque_pop(freeQue);
    return freeCacheBlock;
}

void addFreeCacheBlock(steque_t* freeQue,cacheID* freeBlock)
{
    steque_enqueue(freeQue, freeBlock);
}


/* Main ========================================================= */
int main(int argc, char **argv) {
  int i;
  int option_char = 0;
  unsigned short port = 19121;
  unsigned short nworkerthreads = 8;
  nsegments = 8;
  size_t segsize = 8192;
  char *server = "s3.amazonaws.com/content.udacity-data.com";

  /* disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  /* Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "ixls:t:hn:p:z:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 's': // file-path
        server = optarg;
        break;                                          
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'i':
      case 'x':
      case 'l':
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
    }
  }

  if (segsize < 128) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (!server) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (port < 1024) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }

  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }

  if ((nworkerthreads < 1) || (nworkerthreads > 1219)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }

  // Initialize shared memory set-up here
    serverCache = createShm(nsegments, segsize);
    serverMsq = createMsq();
    wakeUpCacheDaemon(serverMsq,nsegments,segsize);
    steque_t freeQue;
    serverSteque = &freeQue;
    freeQueInit(&freeQue,nsegments);
    handlerArg handlerArgs;
    handlerArgs.messageQueue = serverMsq;
    handlerArgs.shmArray = serverCache;
    handlerArgs.freeQue = &freeQue;
    handlerArgs.segSize = segsize;
    pthread_mutex_init(&handlerArgs.shmQueueMutex, NULL);
    pthread_cond_init(&handlerArgs.freeCacheBlocks, NULL);

  // Initialize server structure here
  gfserver_init(&gfs, nworkerthreads);

  // Set server options here
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 801);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  gfserver_setopt(&gfs, GFS_PORT, port);

  // Set up arguments for worker here
  for(i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, &handlerArgs);
  }
  
  // Invoke the framework - this is an infinite loop and shouldn't return
  gfserver_serve(&gfs);

  // not reached
  return -1;

}
