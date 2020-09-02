#include "gfserver.h"
#include "cache-student.h"

#define BUFSIZE (1219)

static int lockCount = 1;

ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){
    int thisHandlerId = lockCount++;
    //timestamp
    struct timeval thisTime;
    gettimeofday(&thisTime, NULL);
   if (DEBUG_time) printf("server%d: initialization start %s at s: %ld, us:%ld\n",thisHandlerId,path,thisTime.tv_sec,thisTime.tv_usec);
    
    
    handlerArg* IPCdata = (handlerArg*) arg;
    /*
     1. find the next free cache segment.
     2. send the info to the cache daemon.
     3. wait for the daemon to fill up the cache.
     4. transfer the cache to the client.
     5. repeat till all the files have been transfered.
     */

    int bytes_transferred = -1;
    //        send the free shared memory slot.
    //boolean_t needBroadcast = FALSE;
    cacheID* freeCache;
    pthread_mutex_lock(&IPCdata->shmQueueMutex);
    if(DEBUG) printf("server%d: lock mutex\n",thisHandlerId);
    while(steque_isempty(IPCdata->freeQue))
    {
        if(DEBUG) printf("server: %d waiting on cache\n",thisHandlerId);
        pthread_cond_wait(&IPCdata->freeCacheBlocks, &IPCdata->shmQueueMutex);
    }
    freeCache = getFreeCacheBlock(IPCdata->freeQue);
    if(DEBUG) printf("server%d: unlock mutex cache:%d\n",thisHandlerId,freeCache->id);
    pthread_mutex_unlock(&IPCdata->shmQueueMutex);
    freeCache->free = FALSE;
    char msg[100];
    memset(msg, 0, 100);
    snprintf(msg, 100, "path: %s cache: %d",path,freeCache->id);
    if (DEBUG) printf("server%d: %s\n",thisHandlerId,msg);
    sendMsg(IPCdata->messageQueue, msg, 100, SERVER_MSG);
    struct msgContainer* thisMessage = (struct msgContainer*) malloc(sizeof(struct msgContainer)+100+1);
    strcpy(thisMessage->mtext, "");
    receiveMsg(IPCdata->messageQueue, (char*)thisMessage, 100, CACHE_MSG+freeCache->id);

    char* buffer = thisMessage->mtext;
    char* strptr;
    char* word = strtok_r(buffer, " ",&strptr);
    
    //timestamp
      gettimeofday(&thisTime, NULL);
     if (DEBUG_time) printf("server%d: initialization end %s at s: %ld, us:%ld\n",thisHandlerId,path,thisTime.tv_sec,thisTime.tv_usec);
      

    if(strcmp(word, "NOT_FOUND") == 0)
    {
     if(DEBUG) printf("server%d: NOT_FOUND\n",thisHandlerId);
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
    else if(strcmp(word,"FOUND") == 0)
    {
        //timestamp
          gettimeofday(&thisTime, NULL);
         if (DEBUG_time) printf("server%d: transfer start %s at s: %ld, us:%ld\n",thisHandlerId,path,thisTime.tv_sec,thisTime.tv_usec);
        bytes_transferred = 0;
        word = strtok_r(NULL," ",&strptr);
        char* ptr;
        long fileSize = strtol(word, &ptr, 10);
        if(word == ptr)
        {
            printf("server%d: strtol fault, word %s\n",thisHandlerId,word);
            gfs_sendheader(ctx, GF_ERROR, 0);
            return SERVER_FAILURE;
        }
        gfs_sendheader(ctx, GF_OK, fileSize);
        
        size_t write_len;
        int noTransfers = 0;
        
        while(bytes_transferred < fileSize)
        {
            size_t bytesLeft = fileSize - noTransfers * IPCdata->segSize;
            size_t thisTransfer = IPCdata->segSize;
            if (bytesLeft < IPCdata->segSize)
                thisTransfer = bytesLeft;
        setSem(IPCdata->shmArray[freeCache->id], SERVER_SEM, -1);
        write_len = gfs_send(ctx, IPCdata->shmArray[freeCache->id]->data, thisTransfer);
        if (write_len != thisTransfer)
            {
               if(DEBUG) printf( "server%d:handle_with_file write error\n",thisHandlerId);
                return SERVER_FAILURE;
            }
            bytes_transferred += write_len;
//           if(DEBUG) printf("server: %d\n",bytes_transferred);
            noTransfers++;
            setSem(IPCdata->shmArray[freeCache->id], CACHE_SEM, 1);
            
        }
    }
    if (DEBUG) printf("server%d: bytes transferred %d\n",thisHandlerId,bytes_transferred);
    //timestamp
      gettimeofday(&thisTime, NULL);
     if (DEBUG_time) printf("server%d: transfer end %s at s: %ld, us:%ld\n",thisHandlerId,path,thisTime.tv_sec,thisTime.tv_usec);
   
//    int fildes;
//    size_t file_len, bytes_transferred;
//    ssize_t read_len, write_len;
//    char buffer[BUFSIZE];
//    char *data_dir = arg;
//    struct stat statbuf;
//
//    strncpy(buffer,data_dir, BUFSIZE);
//    strncat(buffer,path, BUFSIZE);
//
//    if( 0 > (fildes = open(buffer, O_RDONLY))){
//        if (errno == ENOENT)
//            /* If the file just wasn't found, then send FILE_NOT_FOUND code */
//            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
//        else
//            /* Otherwise, it must have been a server error. gfserver library will handle*/
//            return SERVER_FAILURE;
//    }
//
//    /* Calculating the file size */
//    if (0 > fstat(fildes, &statbuf)) {
//        return SERVER_FAILURE;
//    }
//
//    file_len = (size_t) statbuf.st_size;
//
//    gfs_sendheader(ctx, GF_OK, file_len);
//
////     Sending the file contents chunk by chunk.
//    bytes_transferred = 0;
//    while(bytes_transferred < file_len){
//        read_len = read(fildes, buffer, BUFSIZE);
//        if (read_len <= 0){
//            fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
//            return SERVER_FAILURE;
//        }
//        write_len = gfs_send(ctx, buffer, read_len);
//        if (write_len != read_len){
//            fprintf(stderr, "handle_with_file write error");
//            return SERVER_FAILURE;
//        }
//        bytes_transferred += write_len;
//    }

    free(thisMessage);
    pthread_mutex_lock(&IPCdata->shmQueueMutex);
    freeCache->free = TRUE;
    if(DEBUG) printf("server%d: lock mutex release cache %d\n",thisHandlerId,freeCache->id);
    addFreeCacheBlock(IPCdata->freeQue, freeCache);
 
        if(DEBUG) printf("server%d: broadcasting \n",thisHandlerId);
        pthread_cond_broadcast(&IPCdata->freeCacheBlocks);
  
    if(DEBUG) printf("server%d: unlock mutex post release\n",thisHandlerId);
    pthread_mutex_unlock(&IPCdata->shmQueueMutex);
    if (bytes_transferred < 0)
    {
//        if(DEBUG) printf("server%d: return failure\n",thisHandlerId);
        return SERVER_FAILURE;
    }
    
    //timestamp
      gettimeofday(&thisTime, NULL);
     if (DEBUG_time) printf("server%d: end operation end %s at s: %ld, us:%ld\n",thisHandlerId,path,thisTime.tv_sec,thisTime.tv_usec);
    return bytes_transferred;
}

ssize_t handle_with_cache_old(gfcontext_t *ctx, const char *path, void* arg){
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[BUFSIZE];
	char *data_dir = arg;
	struct stat statbuf;

	strncpy(buffer,data_dir, BUFSIZE);
	strncat(buffer,path, BUFSIZE);

	if( 0 > (fildes = open(buffer, O_RDONLY))){
		if (errno == ENOENT)
			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/ 
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		else
			/* Otherwise, it must have been a server error. gfserver library will handle*/ 
			return SERVER_FAILURE;
	}

	/* Calculating the file size */
	if (0 > fstat(fildes, &statbuf)) {
		return SERVER_FAILURE;
	}

	file_len = (size_t) statbuf.st_size;

	gfs_sendheader(ctx, GF_OK, file_len);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, buffer, BUFSIZE);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return SERVER_FAILURE;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return SERVER_FAILURE;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}

